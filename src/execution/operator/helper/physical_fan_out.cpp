#include "duckdb/execution/operator/helper/physical_fan_out.hpp"
#include "duckdb/execution/execution_context.hpp"
#include "duckdb/parallel/interrupt.hpp"

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
	atomic<idx_t> completed {0};
	atomic<idx_t> accessing {0};

	void Initialize(const vector<LogicalType> &types) {
		for (idx_t i = 0; i < BATCH_SIZE; i++) {
			chunks[i].Initialize(Allocator::DefaultAllocator(), types);
		}
	}

	//! Returns true if all slots have been consumed and no thread is mid-access
	bool IsFullyCompleted() const {
		return count > 0 &&
		       completed.load(std::memory_order_acquire) >= count &&
		       accessing.load(std::memory_order_acquire) == 0;
	}

	void PrepareForFill(const vector<LogicalType> &types) {
		for (idx_t i = 0; i < count; i++) {
			if (chunks[i].ColumnCount() == 0) {
				chunks[i].Initialize(Allocator::DefaultAllocator(), types);
			}
		}
		count = 0;
		next_claim.store(0, std::memory_order_relaxed);
		completed.store(0, std::memory_order_relaxed);
	}
};

class FanOutGlobalSourceState : public GlobalSourceState {
public:
	FanOutGlobalSourceState(ClientContext &context, const PhysicalFanOut &op)
	    : child_types(op.child_source.get().types) {
		child_global = op.child_source.get().GetGlobalSourceState(context);
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

	//! Temp chunk for Produce
	DataChunk temp_chunk;
	bool temp_chunk_initialized = false;

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

	// Register intent to access this buffer before checking anything
	buf.accessing.fetch_add(1, std::memory_order_acq_rel);

	// Verify consume_idx hasn't moved — if it has, the buffer may be recycled
	if (gstate.consume_idx.load(std::memory_order_acquire) != cidx) {
		buf.accessing.fetch_sub(1, std::memory_order_release);
		return false;
	}

	idx_t my_slot = buf.next_claim.fetch_add(1, std::memory_order_acq_rel);
	if (my_slot >= buf.count) {
		buf.accessing.fetch_sub(1, std::memory_order_release);
		return false;
	}

	D_ASSERT(my_slot < BATCH_SIZE);
	chunk.Append(buf.chunks[my_slot]);
	buf.chunks[my_slot].Reset();
	D_ASSERT(chunk.size() > 0);
	lstate.current_batch = buf.batch_indices[my_slot];
	idx_t done = buf.completed.fetch_add(1, std::memory_order_release) + 1;

	// Last consumer advances consume_idx BEFORE releasing accessing.
	// This ensures new consumers see the advanced consume_idx and won't
	// enter this buffer after the producer sees accessing==0.
	if (done >= buf.count) {
		gstate.consume_idx.fetch_add(1, std::memory_order_release);
	}

	// Release our access — producer can now recycle if accessing reaches 0
	buf.accessing.fetch_sub(1, std::memory_order_release);
	return true;
}

//===--------------------------------------------------------------------===//
// Produce — fill buffers from the child source
//===--------------------------------------------------------------------===//
static void Produce(FanOutGlobalSourceState &gstate, const PhysicalFanOut &op, ExecutionContext &context,
                    InterruptState &interrupt_state) {
	while (true) {
		// Check if we've been signaled to stop
		if (gstate.exhausted.load(std::memory_order_acquire)) {
			break;
		}
		// Can we fill? Check that the target buffer is fully completed
		auto pidx = gstate.produce_idx.load(std::memory_order_relaxed);
		auto &fill_buf = gstate.buffers[pidx % NUM_BUFFERS];
		// For the first fill (count==0) the buffer is unused. For subsequent fills,
		// wait until all consumers have completed reading from it.
		if (fill_buf.count > 0 && !fill_buf.IsFullyCompleted()) {
			break; // buffer still in use by consumers
		}
		fill_buf.PrepareForFill(gstate.child_types);

		// Initialize temp chunk on first use
		if (!gstate.temp_chunk_initialized) {
			gstate.temp_chunk.Initialize(Allocator::DefaultAllocator(), gstate.child_types);
			gstate.temp_chunk_initialized = true;
		}

		// Fill at full speed — no atomics in this loop
		bool done = false;
		while (fill_buf.count < BATCH_SIZE && !done) {
			gstate.temp_chunk.Reset();

			OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, interrupt_state};
			auto result = op.child_source.get().GetData(context, gstate.temp_chunk, child_input);

			// Deep copy into buffer so source can reuse its internal buffers
			if (gstate.temp_chunk.size() > 0) {
				D_ASSERT(fill_buf.count < BATCH_SIZE);
				auto &dest = fill_buf.chunks[fill_buf.count];
				dest.Reset();
				dest.Append(gstate.temp_chunk);
				gstate.temp_chunk.Reset();
				fill_buf.batch_indices[fill_buf.count] = gstate.next_batch++;
				fill_buf.count++;
			}

			// Handle return code
			switch (result) {
			case SourceResultType::FINISHED:
				gstate.exhausted.store(true, std::memory_order_release);
				done = true;
				break;
			case SourceResultType::BLOCKED:
				done = true;
				break;
			case SourceResultType::HAVE_MORE_OUTPUT:
				break;
			}
		}

		if (fill_buf.count > 0) {
			// Publish — single store makes buffer visible to consumers
			idx_t new_pidx = gstate.produce_idx.load(std::memory_order_relaxed) + 1;
			gstate.produce_idx.store(new_pidx, std::memory_order_release);
		}

		if (gstate.exhausted.load(std::memory_order_relaxed)) {
			break;
		}
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
			return SourceResultType::FINISHED;
		}
		OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, input.interrupt_state};
		auto result = child_source.get().GetData(context, chunk, child_input);
		if (chunk.size() > 0) {
			lstate.current_batch = gstate.next_passthrough_batch++;
		}
		if (result == SourceResultType::FINISHED) {
			return SourceResultType::FINISHED;
		}
		gstate.claimed.store(false, std::memory_order_release);
		return result;
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

	// No data available yet and not producing — tell executor to retry
	return SourceResultType::HAVE_MORE_OUTPUT;
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
