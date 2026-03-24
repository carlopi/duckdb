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

static constexpr idx_t BATCH_SIZE = 8;

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
	idx_t count = 0;               //! How many chunks were filled
	atomic<idx_t> next_claim {0};   //! Consumer CAS to claim
	atomic<idx_t> done_count {0};   //! Consumer increments after Move
	atomic<BufferState> state {BufferState::NOT_INITIALIZED};

	void Initialize(const vector<LogicalType> &types) {
		for (idx_t i = 0; i < BATCH_SIZE; i++) {
			chunks[i].Initialize(Allocator::DefaultAllocator(), types);
		}
		state.store(BufferState::READY_TO_BE_FILLED, std::memory_order_release);
	}

	//! Reset counters for next fill cycle. No re-Initialize needed — Reference doesn't destroy chunks.
	void PrepareForFill() {
		count = 0;
		next_claim.store(0, std::memory_order_relaxed);
		done_count.store(0, std::memory_order_relaxed);
	}

	bool AllDone() const {
		return done_count.load(std::memory_order_acquire) >= count;
	}
};

class FanOutGlobalSourceState : public GlobalSourceState {
public:
	FanOutGlobalSourceState(ClientContext &context, const PhysicalFanOut &op)
	    : child_types(op.child_source.get().types) {
		child_global = op.child_source.get().GetGlobalSourceState(context);
		buffers[0].Initialize(child_types);
		buffers[1].Initialize(child_types);
	}

	unique_ptr<GlobalSourceState> child_global;
	unique_ptr<LocalSourceState> child_local;
	bool child_local_initialized = false;
	mutex init_lock;
	const vector<LogicalType> &child_types;

	//! Double buffer
	ChunkBuffer buffers[2];
	atomic<int> active_buffer {-1}; //! Which buffer consumers read from (-1 = none)

	//! Producer
	atomic<bool> producing {false};
	atomic<bool> exhausted {false};
	idx_t next_batch = 0;
	idx_t fill_idx = 0; //! Which buffer to fill next (only producer)

	//! Blocked consumers
	StateWithBlockableTasks blocked_state;

	idx_t MaxThreads() override {
		return NumericLimits<idx_t>::Maximum();
	}
};

class FanOutLocalSourceState : public LocalSourceState {
public:
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
	lock_guard<mutex> lock(fan_gstate.init_lock);
	if (!fan_gstate.child_local_initialized) {
		fan_gstate.child_local = child_source.get().GetLocalSourceState(context, *fan_gstate.child_global);
		fan_gstate.child_local_initialized = true;
	}
	return make_uniq<FanOutLocalSourceState>();
}

//===--------------------------------------------------------------------===//
// TryConsume — CAS on next_claim of active buffer, Move, increment done_count
//===--------------------------------------------------------------------===//
static bool TryConsume(FanOutGlobalSourceState &gstate, FanOutLocalSourceState &lstate, DataChunk &chunk) {
	int active = gstate.active_buffer.load(std::memory_order_acquire);
	if (active < 0) {
		return false;
	}
	auto &buf = gstate.buffers[active];
	if (buf.state.load(std::memory_order_acquire) != BufferState::IN_PROCESSING) {
		return false;
	}
	idx_t my_slot = buf.next_claim.fetch_add(1, std::memory_order_acq_rel);
	if (my_slot >= buf.count) {
		return false;
	}
	chunk.Reference(buf.chunks[my_slot]);
	lstate.current_batch = buf.batch_indices[my_slot];
	buf.done_count.fetch_add(1, std::memory_order_release);

	// If we were the last consumer, transition buffer back to READY_TO_BE_FILLED
	if (buf.done_count.load(std::memory_order_acquire) >= buf.count) {
		buf.state.store(BufferState::READY_TO_BE_FILLED, std::memory_order_release);
	}
	return true;
}

//===--------------------------------------------------------------------===//
// Produce — fill a buffer, publish it
//===--------------------------------------------------------------------===//
static void Produce(FanOutGlobalSourceState &gstate, const PhysicalFanOut &op, ExecutionContext &context,
                    InterruptState &interrupt_state) {
	while (true) {
		// Find a buffer that's READY_TO_BE_FILLED
		auto &buf = gstate.buffers[gstate.fill_idx];
		auto expected_state = BufferState::READY_TO_BE_FILLED;
		if (!buf.state.compare_exchange_strong(expected_state, BufferState::FILLING, std::memory_order_acq_rel)) {
			// Buffer not ready (still being processed) — try the other one
			gstate.fill_idx = 1 - gstate.fill_idx;
			auto &buf2 = gstate.buffers[gstate.fill_idx];
			expected_state = BufferState::READY_TO_BE_FILLED;
			if (!buf2.state.compare_exchange_strong(expected_state, BufferState::FILLING, std::memory_order_acq_rel)) {
				// Both buffers busy — break, consumers need to drain
				break;
			}
		}

		auto &fill_buf = gstate.buffers[gstate.fill_idx];

		// Re-initialize any Move'd chunks + reset counters
		fill_buf.PrepareForFill();

		// Fill: Reset + GetData into pre-allocated chunks (no malloc)
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
			// Wait for old active buffer to be fully processed before swapping
			int old_active = gstate.active_buffer.load(std::memory_order_acquire);
			if (old_active >= 0) {
				auto &old_buf = gstate.buffers[old_active];
				while (old_buf.state.load(std::memory_order_acquire) == BufferState::IN_PROCESSING) {
					// spin — consumers finishing
				}
			}

			// Publish: set state and swap active
			fill_buf.state.store(BufferState::IN_PROCESSING, std::memory_order_release);
			gstate.active_buffer.store(gstate.fill_idx, std::memory_order_release);
			gstate.fill_idx = 1 - gstate.fill_idx;
		} else {
			// Nothing produced — reset state
			fill_buf.state.store(BufferState::READY_TO_BE_FILLED, std::memory_order_release);
		}

		// Wake consumers
		{
			annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
			gstate.blocked_state.UnblockTasks();
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
