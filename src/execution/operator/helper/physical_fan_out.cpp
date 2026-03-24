#include "duckdb/execution/operator/helper/physical_fan_out.hpp"
#include "duckdb/execution/execution_context.hpp"
#include "duckdb/parallel/interrupt.hpp"
#include "duckdb/parallel/pipeline.hpp"

namespace duckdb {

PhysicalFanOut::PhysicalFanOut(PhysicalPlan &plan, PhysicalOperator &child_source_p, idx_t estimated_cardinality)
    : PhysicalOperator(plan, PhysicalOperatorType::EXTENSION, child_source_p.types, estimated_cardinality),
      child_source(child_source_p) {
}

//===--------------------------------------------------------------------===//
// State
//===--------------------------------------------------------------------===//

static constexpr idx_t BATCH_SIZE = 128;
static constexpr idx_t NUM_BUFFERS = 8;

enum class BufferState : uint8_t {
	NOT_INITIALIZED,
	READY_TO_BE_FILLED,
	FILLING,
	READY_TO_BE_PROCESSED,
	IN_PROCESSING
};

struct ChunkBuffer {
	DataChunk chunks[BATCH_SIZE];
	idx_t batch_indices[BATCH_SIZE];
	idx_t count = 0;
	atomic<idx_t> next_claim {0};
	atomic<idx_t> done_count {0};
	atomic<BufferState> state {BufferState::NOT_INITIALIZED};

	void Initialize(const vector<LogicalType> &types) {
		for (idx_t i = 0; i < BATCH_SIZE; i++) {
			chunks[i].Initialize(Allocator::DefaultAllocator(), types);
		}
		state.store(BufferState::READY_TO_BE_FILLED, std::memory_order_release);
	}

	void PrepareForFill() {
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
		for (idx_t i = 0; i < NUM_BUFFERS; i++) {
			buffers[i].Initialize(child_types);
		}
	}

	unique_ptr<GlobalSourceState> child_global;
	unique_ptr<LocalSourceState> child_local;
	bool child_local_initialized = false;
	mutex init_lock;
	const vector<LogicalType> &child_types;

	//! N buffers — producer fills in order, consumers read in order
	ChunkBuffer buffers[NUM_BUFFERS];
	atomic<idx_t> consume_idx {0}; //! Which buffer consumers should read from (in order)

	//! Producer
	atomic<bool> producing {false};
	atomic<bool> exhausted {false};
	idx_t next_batch = 0;
	idx_t produce_idx = 0; //! Next buffer to fill (only producer, in order)

	//! Blocked consumers
	StateWithBlockableTasks blocked_state;
	atomic<bool> has_blocked {false};
	atomic<idx_t> next_thread_id {0};

	idx_t MaxThreads() override {
		return NumericLimits<idx_t>::Maximum();
	}
};

//! Threads below this ID yield, threads at or above block
static constexpr idx_t YIELD_THREAD_LIMIT = 4;

class FanOutLocalSourceState : public LocalSourceState {
public:
	idx_t current_batch = 0;
	idx_t thread_id = 0;
	bool id_assigned = false;
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
	lock_guard<mutex> lock(fan_gstate.init_lock);
	if (!fan_gstate.child_local_initialized) {
		fan_gstate.child_local = child_source.get().GetLocalSourceState(context, *fan_gstate.child_global);
		fan_gstate.child_local_initialized = true;
	}
	return make_uniq<FanOutLocalSourceState>();
}

//===--------------------------------------------------------------------===//
// TryConsume — scan buffers for IN_PROCESSING, CAS to claim a slot
//===--------------------------------------------------------------------===//
static bool TryConsume(FanOutGlobalSourceState &gstate, FanOutLocalSourceState &lstate, DataChunk &chunk) {
	idx_t cidx = gstate.consume_idx.load(std::memory_order_acquire);
	auto &buf = gstate.buffers[cidx % NUM_BUFFERS];
	if (buf.state.load(std::memory_order_acquire) != BufferState::IN_PROCESSING) {
		return false;
	}
	idx_t my_slot = buf.next_claim.fetch_add(1, std::memory_order_acq_rel);
	if (my_slot >= buf.count) {
		return false;
	}
	chunk.Reference(buf.chunks[my_slot]);
	lstate.current_batch = buf.batch_indices[my_slot];
	idx_t done = buf.done_count.fetch_add(1, std::memory_order_release) + 1;

	// Last consumer: transition buffer to READY_TO_BE_FILLED and advance consume_idx
	if (done >= buf.count) {
		buf.state.store(BufferState::READY_TO_BE_FILLED, std::memory_order_release);
		gstate.consume_idx.fetch_add(1, std::memory_order_release);
	}
	return true;
}

//===--------------------------------------------------------------------===//
// Produce — fill READY_TO_BE_FILLED buffers, publish as IN_PROCESSING
//===--------------------------------------------------------------------===//
static void Produce(FanOutGlobalSourceState &gstate, const PhysicalFanOut &op, ExecutionContext &context,
                    InterruptState &interrupt_state) {
	while (true) {
		// Find a READY_TO_BE_FILLED buffer (round-robin)
		// Fill in order: try the next buffer in sequence
		auto &fill_buf = gstate.buffers[gstate.produce_idx % NUM_BUFFERS];
		auto expected = BufferState::READY_TO_BE_FILLED;
		if (!fill_buf.state.compare_exchange_strong(expected, BufferState::FILLING, std::memory_order_acq_rel)) {
			break; // next buffer not ready yet — consumers haven't drained it
		}
		fill_buf.PrepareForFill();

		// Fill at full speed — Reset + GetData into pre-allocated chunks
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
			// Publish and advance
			fill_buf.state.store(BufferState::IN_PROCESSING, std::memory_order_release);
			gstate.produce_idx++;
		} else {
			fill_buf.state.store(BufferState::READY_TO_BE_FILLED, std::memory_order_release);
		}

		// Wake consumers only if any are blocked (skip lock otherwise)
		if (gstate.has_blocked.load(std::memory_order_acquire)) {
			annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
			gstate.blocked_state.UnblockTasks();
			gstate.has_blocked.store(false, std::memory_order_release);
		}

		if (gstate.exhausted.load(std::memory_order_acquire)) {
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

	// Assign thread ID on first miss
	if (!lstate.id_assigned) {
		lstate.thread_id = gstate.next_thread_id.fetch_add(1, std::memory_order_relaxed);
		lstate.id_assigned = true;
	}

	// Low-ID threads: yield (cheap retry, stays responsive)
	// High-ID threads: block (saves CPU for producer + low-ID threads)
	if (lstate.thread_id < YIELD_THREAD_LIMIT) {
		return SourceResultType::HAVE_MORE_OUTPUT;
	}

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
                                                       GlobalSourceState &gstate, LocalSourceState &lstate,
                                                       const OperatorPartitionInfo &partition_info) const {
	auto &fan_out_lstate = lstate.Cast<FanOutLocalSourceState>();
	return OperatorPartitionData(fan_out_lstate.current_batch);
}

} // namespace duckdb
