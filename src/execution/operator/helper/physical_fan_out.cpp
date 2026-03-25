#include "duckdb/execution/operator/helper/physical_fan_out.hpp"
#include "duckdb/execution/execution_context.hpp"
#include "duckdb/parallel/interrupt.hpp"
#include "duckdb/parallel/pipeline.hpp"

namespace duckdb {

PhysicalFanOut::PhysicalFanOut(PhysicalPlan &plan, PhysicalOperator &child_source_p, idx_t estimated_cardinality)
    : PhysicalOperator(plan, PhysicalOperatorType::FAN_OUT, child_source_p.types, estimated_cardinality),
      child_source(child_source_p) {
}

//===--------------------------------------------------------------------===//
// State
//===--------------------------------------------------------------------===//

static constexpr idx_t BATCH_SIZE = 128;
static constexpr idx_t NUM_BUFFERS = 8;

struct ChunkBuffer {
	DataChunk chunks[BATCH_SIZE];
	idx_t batch_indices[BATCH_SIZE];
	idx_t count = 0;
	atomic<idx_t> next_claim {0};
	atomic<idx_t> done_count {0};

	void Initialize(const vector<LogicalType> &types) {
		for (idx_t i = 0; i < BATCH_SIZE; i++) {
			chunks[i].Initialize(Allocator::DefaultAllocator(), types);
		}
	}

	void PrepareForFill(const vector<LogicalType> &types) {
		for (idx_t i = 0; i < count; i++) {
			if (chunks[i].ColumnCount() == 0) {
				chunks[i].Initialize(Allocator::DefaultAllocator(), types);
			}
		}
		count = 0;
		next_claim.store(0, std::memory_order_relaxed);
		done_count.store(0, std::memory_order_relaxed);
	}
};

class FanOutGlobalSourceState : public GlobalSourceState {
public:
	FanOutGlobalSourceState(ClientContext &context, const PhysicalFanOut &op)
	    : child_types(op.child_source.get().types) {
		child_global = op.child_source.get().GetGlobalSourceState(context);
		// Passthrough if child already supports multiple threads, or forced by plan
		force_passthrough = op.force_passthrough;
		passthrough = force_passthrough || child_global->MaxThreads() > 1;
		if (!passthrough) {
			for (idx_t i = 0; i < NUM_BUFFERS; i++) {
				buffers[i].Initialize(child_types);
			}
		}
	}

	//! If true, delegate directly to the child source (no buffering)
	bool passthrough = false;
	//! If true, only one thread runs — the rest return FINISHED immediately
	bool force_passthrough = false;

	unique_ptr<GlobalSourceState> child_global;
	unique_ptr<LocalSourceState> child_local;
	bool child_local_initialized = false;
	mutex init_lock;
	const vector<LogicalType> &child_types;

	//! Force-passthrough: first thread claims this, others see true and return FINISHED
	atomic<bool> claimed {false};
	//! Batch counter for force-passthrough mode
	idx_t next_passthrough_batch = 0;

	//! N buffers — producer fills in order, consumers read in order
	ChunkBuffer buffers[NUM_BUFFERS];
	atomic<idx_t> consume_idx {0};

	//! Producer
	atomic<bool> producing {false};
	atomic<bool> exhausted {false};
	idx_t next_batch = 0;
	atomic<idx_t> produce_idx {0};

	//! Blocked consumers
	StateWithBlockableTasks blocked_state;
	atomic<bool> has_blocked {false};

	idx_t MaxThreads() override {
		if (force_passthrough) {
			return 1;
		}
		if (passthrough) {
			return child_global->MaxThreads();
		}
		return NumericLimits<idx_t>::Maximum();
	}
};

class FanOutLocalSourceState : public LocalSourceState {
public:
	//! In passthrough mode, this holds the child's local state
	unique_ptr<LocalSourceState> child_local;
	idx_t current_batch = 0;
};

//===--------------------------------------------------------------------===//
// Init
//===--------------------------------------------------------------------===//
unique_ptr<GlobalSourceState> PhysicalFanOut::GetGlobalSourceState(ClientContext &context) const {
	return make_uniq_base<GlobalSourceState, FanOutGlobalSourceState>(context, *this);
}

unique_ptr<LocalSourceState> PhysicalFanOut::GetLocalSourceState(ExecutionContext &context,
                                                                 GlobalSourceState &gstate) const {
	auto &fan_gstate = gstate.Cast<FanOutGlobalSourceState>();
	auto result = make_uniq<FanOutLocalSourceState>();
	if (fan_gstate.passthrough && !fan_gstate.force_passthrough) {
		// Multi-thread passthrough: each thread gets its own child local state
		result->child_local = child_source.get().GetLocalSourceState(context, *fan_gstate.child_global);
	} else {
		// Fan-out or force-passthrough: single shared child local state
		lock_guard<mutex> lock(fan_gstate.init_lock);
		if (!fan_gstate.child_local_initialized) {
			fan_gstate.child_local = child_source.get().GetLocalSourceState(context, *fan_gstate.child_global);
			fan_gstate.child_local_initialized = true;
		}
	}
	return std::move(result);
}

//===--------------------------------------------------------------------===//
// TryConsume — claim a slot from the current consume buffer
//===--------------------------------------------------------------------===//
static bool TryConsume(FanOutGlobalSourceState &gstate, FanOutLocalSourceState &lstate, DataChunk &chunk) {
	idx_t cidx = gstate.consume_idx.load(std::memory_order_acquire);
	idx_t pidx = gstate.produce_idx.load(std::memory_order_acquire);
	if (cidx >= pidx) {
		return false; // no filled buffers
	}
	auto &buf = gstate.buffers[cidx % NUM_BUFFERS];
	idx_t my_slot = buf.next_claim.fetch_add(1, std::memory_order_acq_rel);
	if (my_slot >= buf.count) {
		return false;
	}
	chunk.Move(buf.chunks[my_slot]);
	lstate.current_batch = buf.batch_indices[my_slot];
	idx_t done = buf.done_count.fetch_add(1, std::memory_order_release) + 1;

	// Last consumer advances consume_idx
	if (done >= buf.count) {
		gstate.consume_idx.fetch_add(1, std::memory_order_release);
	}
	return true;
}

//===--------------------------------------------------------------------===//
// Produce — fill buffers from the child source
//===--------------------------------------------------------------------===//
static void Produce(FanOutGlobalSourceState &gstate, const PhysicalFanOut &op, ExecutionContext &context,
                    InterruptState &interrupt_state) {
	while (true) {
		// Can we fill? Check counter gap — no CAS needed, producer is sole writer
		idx_t cidx = gstate.consume_idx.load(std::memory_order_acquire);
		if (gstate.produce_idx.load(std::memory_order_relaxed) - cidx >= NUM_BUFFERS) {
			break; // all buffers full
		}

		auto &fill_buf = gstate.buffers[gstate.produce_idx.load(std::memory_order_relaxed) % NUM_BUFFERS];
		fill_buf.PrepareForFill(gstate.child_types);

		// Fill at full speed — no atomics in this loop
		for (idx_t i = 0; i < BATCH_SIZE; i++) {
			fill_buf.chunks[i].Reset();

			OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, interrupt_state};
			auto result = op.child_source.get().GetData(context, fill_buf.chunks[i], child_input);

			if (result == SourceResultType::FINISHED || fill_buf.chunks[i].size() == 0) {
				gstate.exhausted.store(true, std::memory_order_release);
				break;
			}

			fill_buf.batch_indices[i] = gstate.next_batch++;
			fill_buf.count++;
		}

		if (fill_buf.count > 0) {
			// Publish — single store makes buffer visible to consumers
			idx_t new_pidx = gstate.produce_idx.load(std::memory_order_relaxed) + 1;
			gstate.produce_idx.store(new_pidx, std::memory_order_release);

			// Wake every 4 buffers to let consumers start working
			if (new_pidx % 4 == 0 && gstate.has_blocked.load(std::memory_order_relaxed)) {
				annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
				gstate.blocked_state.UnblockTasks();
				gstate.has_blocked.store(false, std::memory_order_release);
			}
		}

		if (gstate.exhausted.load(std::memory_order_relaxed)) {
			break;
		}
	}

	// Final wake after produce loop
	if (gstate.has_blocked.load(std::memory_order_acquire)) {
		annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
		gstate.blocked_state.UnblockTasks();
		gstate.has_blocked.store(false, std::memory_order_release);
	}

	gstate.producing.store(false, std::memory_order_release);
}

//===--------------------------------------------------------------------===//
// GetData
//===--------------------------------------------------------------------===//
SourceResultType PhysicalFanOut::GetDataInternal(ExecutionContext &context, DataChunk &chunk,
                                                 OperatorSourceInput &input) const {
	auto &gstate = input.global_state.Cast<FanOutGlobalSourceState>();
	auto &lstate = input.local_state.Cast<FanOutLocalSourceState>();

	// Force-passthrough: source→sink with no intermediates, single thread only
	if (gstate.force_passthrough) {
		bool expected = false;
		if (!gstate.claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
			// Another thread already claimed — we're done
			return SourceResultType::FINISHED;
		}
		// We're the sole thread — call child directly
		OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, input.interrupt_state};
		auto result = child_source.get().GetData(context, chunk, child_input);
		lstate.current_batch = gstate.next_passthrough_batch++;
		if (result == SourceResultType::FINISHED) {
			return SourceResultType::FINISHED;
		}
		// Release claim so we can be called again
		gstate.claimed.store(false, std::memory_order_release);
		return SourceResultType::HAVE_MORE_OUTPUT;
	}

	// Passthrough: child source already supports parallelism
	if (gstate.passthrough) {
		OperatorSourceInput child_input {*gstate.child_global, *lstate.child_local, input.interrupt_state};
		return child_source.get().GetData(context, chunk, child_input);
	}

	// Fan-out mode: single-threaded child, multi-threaded consumers
	if (TryConsume(gstate, lstate, chunk)) {
		return SourceResultType::HAVE_MORE_OUTPUT;
	}

	if (gstate.exhausted.load(std::memory_order_acquire)) {
		if (TryConsume(gstate, lstate, chunk)) {
			return SourceResultType::HAVE_MORE_OUTPUT;
		}
		return SourceResultType::FINISHED;
	}

	bool expected = false;
	if (gstate.producing.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
		Produce(gstate, *this, context, input.interrupt_state);

		if (TryConsume(gstate, lstate, chunk)) {
			return SourceResultType::HAVE_MORE_OUTPUT;
		}
		return gstate.exhausted.load(std::memory_order_acquire) ? SourceResultType::FINISHED
		                                                        : SourceResultType::HAVE_MORE_OUTPUT;
	}

	// Block
	gstate.has_blocked.store(true, std::memory_order_release);
	annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
	if (TryConsume(gstate, lstate, chunk)) {
		return SourceResultType::HAVE_MORE_OUTPUT;
	}
	if (gstate.exhausted.load(std::memory_order_acquire)) {
		return SourceResultType::FINISHED;
	}
	return gstate.blocked_state.BlockSource(input.interrupt_state);
}

//===--------------------------------------------------------------------===//
// Partition data
//===--------------------------------------------------------------------===//
OperatorPartitionData PhysicalFanOut::GetPartitionData(ExecutionContext &context, DataChunk &chunk,
                                                       GlobalSourceState &gstate_p, LocalSourceState &lstate_p,
                                                       const OperatorPartitionInfo &partition_info) const {
	auto &gstate = gstate_p.Cast<FanOutGlobalSourceState>();
	if (gstate.passthrough && !gstate.force_passthrough) {
		return child_source.get().GetPartitionData(
		    context, chunk, *gstate.child_global, *lstate_p.Cast<FanOutLocalSourceState>().child_local, partition_info);
	}
	auto &fan_out_lstate = lstate_p.Cast<FanOutLocalSourceState>();
	return OperatorPartitionData(fan_out_lstate.current_batch);
}

} // namespace duckdb
