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

	//! Ring buffer — consumers use atomic read_pos, producer uses atomic write_pos
	BufferSlot slots[BUFFER_CAPACITY];
	atomic<idx_t> write_pos {0}; //! Published by producer after filling a batch
	atomic<idx_t> read_pos {0};  //! Consumers fetch_add to claim

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
	idx_t my_pos = gstate.read_pos.fetch_add(1, std::memory_order_acq_rel);
	if (my_pos >= gstate.write_pos.load(std::memory_order_acquire)) {
		// Nothing available — undo
		gstate.read_pos.fetch_sub(1, std::memory_order_relaxed);
		return false;
	}
	auto &slot = gstate.slots[my_pos % BUFFER_CAPACITY];
	D_ASSERT(slot.chunk);
	chunk.Move(*slot.chunk);
	slot.chunk.reset();
	lstate.current_batch = slot.batch_index;
	return true;
}

//===--------------------------------------------------------------------===//
// Produce — one lock acquisition at end to wake consumers, rest is lock-free
//===--------------------------------------------------------------------===//
static void Produce(FanOutGlobalSourceState &gstate, const PhysicalFanOut &op, ExecutionContext &context,
                    InterruptState &interrupt_state) {
	while (true) {
		// 1. Snapshot positions (atomic reads, no lock)
		idx_t cur_write = gstate.write_pos.load(std::memory_order_relaxed);
		idx_t cur_read = gstate.read_pos.load(std::memory_order_acquire);
		idx_t available = BUFFER_CAPACITY - (cur_write - cur_read);
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
