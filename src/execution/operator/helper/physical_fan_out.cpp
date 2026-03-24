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

static constexpr idx_t BUFFER_CAPACITY = 8;

struct BufferSlot {
	unique_ptr<DataChunk> chunk;
	idx_t batch_index = 0;
};

class FanOutGlobalSourceState : public GlobalSourceState {
public:
	FanOutGlobalSourceState(ClientContext &context, const PhysicalFanOut &op)
	    : child_types(op.child_source.get().types) {
		child_global = op.child_source.get().GetGlobalSourceState(context);
	}

	unique_ptr<GlobalSourceState> child_global;
	unique_ptr<LocalSourceState> child_local;
	bool child_local_initialized = false;
	mutex init_lock;
	const vector<LogicalType> &child_types;

	//! Ring buffer
	BufferSlot slots[BUFFER_CAPACITY];
	atomic<idx_t> write_pos {0}; //! Producer publishes after filling
	atomic<idx_t> read_pos {0};  //! Consumers CAS to claim a slot
	atomic<idx_t> done_pos {0};  //! Consumers increment AFTER Move complete — producer uses this for available

	//! Producer
	atomic<bool> producing {false};
	atomic<bool> exhausted {false};
	idx_t next_batch = 0;

	//! Blocked consumers (only lock when blocking)
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
// TryConsume — lock-free: atomic fetch_add on read_pos
//===--------------------------------------------------------------------===//
static bool TryConsume(FanOutGlobalSourceState &gstate, FanOutLocalSourceState &lstate, DataChunk &chunk) {
	idx_t cur_read = gstate.read_pos.load(std::memory_order_relaxed);
	while (true) {
		idx_t cur_write = gstate.write_pos.load(std::memory_order_acquire);
		if (cur_read >= cur_write) {
			return false; // nothing available
		}
		// Try to claim this position
		if (gstate.read_pos.compare_exchange_weak(cur_read, cur_read + 1, std::memory_order_acq_rel)) {
			auto &slot = gstate.slots[cur_read % BUFFER_CAPACITY];
			chunk.Move(*slot.chunk);
			slot.chunk.reset();
			lstate.current_batch = slot.batch_index;
			// Signal: Move complete, producer can reuse this physical slot
			gstate.done_pos.fetch_add(1, std::memory_order_release);
			return true;
		}
		// CAS failed — cur_read updated, retry
	}
}

//===--------------------------------------------------------------------===//
// Produce — one lock acquisition at end to wake consumers, rest is lock-free
//===--------------------------------------------------------------------===//
static void Produce(FanOutGlobalSourceState &gstate, const PhysicalFanOut &op, ExecutionContext &context,
                    InterruptState &interrupt_state) {
	while (true) {
		// 1. Snapshot positions (atomic reads, no lock)
		//    Use done_pos (not read_pos) — a slot is only safe to reuse after Move completes
		idx_t cur_write = gstate.write_pos.load(std::memory_order_relaxed);
		idx_t cur_done = gstate.done_pos.load(std::memory_order_acquire);
		idx_t available = BUFFER_CAPACITY - (cur_write - cur_done);
		if (available == 0) {
			break;
		}

		// 2. Fill all available slots — no locks, no atomics in this loop
		idx_t filled = 0;
		for (idx_t i = 0; i < available; i++) {
			auto temp_chunk = make_uniq<DataChunk>();
			temp_chunk->Initialize(Allocator::DefaultAllocator(), gstate.child_types);

			OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, interrupt_state};
			auto result = op.child_source.get().GetData(context, *temp_chunk, child_input);

			if (result == SourceResultType::FINISHED || temp_chunk->size() == 0) {
				gstate.exhausted.store(true, std::memory_order_release);
				break;
			}

			auto &slot = gstate.slots[(cur_write + i) % BUFFER_CAPACITY];
			slot.chunk = std::move(temp_chunk);
			slot.batch_index = gstate.next_batch++;
			filled++;
		}

		// 3. Publish all at once — single atomic store
		if (filled > 0) {
			gstate.write_pos.store(cur_write + filled, std::memory_order_release);
		}

		// 4. One lock: wake blocked consumers
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

	// 1. Try to grab (lock-free)
	if (TryConsume(gstate, lstate, chunk)) {
		return SourceResultType::HAVE_MORE_OUTPUT;
	}

	// 2. Done?
	if (gstate.exhausted.load(std::memory_order_acquire)) {
		if (TryConsume(gstate, lstate, chunk)) {
			return SourceResultType::HAVE_MORE_OUTPUT;
		}
		return SourceResultType::FINISHED;
	}

	// 3. Become producer (lock-free CAS)
	bool expected = false;
	if (gstate.producing.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
		Produce(gstate, *this, context, input.interrupt_state);

		if (TryConsume(gstate, lstate, chunk)) {
			return SourceResultType::HAVE_MORE_OUTPUT;
		}
		return gstate.exhausted.load(std::memory_order_acquire) ? SourceResultType::FINISHED
		                                                        : SourceResultType::HAVE_MORE_OUTPUT;
	}

	// 4. Block (only lock)
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
