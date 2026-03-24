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

//! Number of chunk slots in the buffer
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

	//! The child source's global state (single instance, used only by producer)
	unique_ptr<GlobalSourceState> child_global;
	//! A single shared child local state (used only by producer)
	unique_ptr<LocalSourceState> child_local;
	//! Whether child_local has been initialized
	bool child_local_initialized = false;
	//! Mutex for child_local initialization
	mutex init_lock;
	//! Child types for allocating chunks
	const vector<LogicalType> &child_types;

	//! --- Buffer ---
	mutex buffer_lock;
	BufferSlot slots[BUFFER_CAPACITY];
	idx_t buffer_count = 0; //! Number of filled slots available for consumers
	idx_t buffer_read = 0;  //! Next slot to read from

	//! --- Producer ---
	atomic<bool> producing {false}; //! Is a thread currently filling the buffer?
	atomic<bool> exhausted {false}; //! Source is done
	idx_t next_batch = 0;          //! Next batch index (only touched by producer)

	//! --- Blocked consumers waiting for slots ---
	StateWithBlockableTasks blocked_state;

	idx_t MaxThreads() override {
		return NumericLimits<idx_t>::Maximum();
	}
};

class FanOutLocalSourceState : public LocalSourceState {
public:
	//! Batch index assigned to the most recent chunk from this thread
	idx_t current_batch = 0;
};

//===--------------------------------------------------------------------===//
// GetGlobalSourceState / GetLocalSourceState
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
// TryConsume — try to grab a chunk from the buffer
//===--------------------------------------------------------------------===//
static bool TryConsume(FanOutGlobalSourceState &gstate, FanOutLocalSourceState &lstate, DataChunk &chunk) {
	lock_guard<mutex> lock(gstate.buffer_lock);
	if (gstate.buffer_read < gstate.buffer_count) {
		auto &slot = gstate.slots[gstate.buffer_read % BUFFER_CAPACITY];
		chunk.Move(*slot.chunk);
		lstate.current_batch = slot.batch_index;
		gstate.buffer_read++;
		return true;
	}
	return false;
}

//===--------------------------------------------------------------------===//
// Produce — fill the buffer, waking one consumer per slot
//===--------------------------------------------------------------------===//
static void Produce(FanOutGlobalSourceState &gstate, const PhysicalFanOut &op, ExecutionContext &context,
                    InterruptState &interrupt_state) {
	while (true) {
		// Check if buffer is full
		{
			lock_guard<mutex> lock(gstate.buffer_lock);
			if (gstate.buffer_count - gstate.buffer_read >= BUFFER_CAPACITY) {
				break; // buffer full
			}
		}

		// Call child GetData (no lock needed — only one producer at a time)
		auto temp_chunk = make_uniq<DataChunk>();
		temp_chunk->Initialize(Allocator::DefaultAllocator(), gstate.child_types);

		OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, interrupt_state};
		auto result = op.child_source.get().GetData(context, *temp_chunk, child_input);

		if (result == SourceResultType::FINISHED || temp_chunk->size() == 0) {
			gstate.exhausted.store(true);
			// Wake all remaining blocked consumers so they can see exhausted
			annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
			gstate.blocked_state.UnblockTasks();
			break;
		}

		// Put chunk in buffer and wake one consumer
		{
			lock_guard<mutex> lock(gstate.buffer_lock);
			auto idx = gstate.buffer_count % BUFFER_CAPACITY;
			gstate.slots[idx].chunk = std::move(temp_chunk);
			gstate.slots[idx].batch_index = gstate.next_batch++;
			gstate.buffer_count++;
		}

		// Wake one blocked consumer
		{
			annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
			gstate.blocked_state.UnblockTasks(); // wakes all, but that's fine — they'll grab slots or re-block
		}
	}

	gstate.producing.store(false);
}

//===--------------------------------------------------------------------===//
// GetData
//===--------------------------------------------------------------------===//
SourceResultType PhysicalFanOut::GetDataInternal(ExecutionContext &context, DataChunk &chunk,
                                                 OperatorSourceInput &input) const {
	auto &gstate = input.global_state.Cast<FanOutGlobalSourceState>();
	auto &lstate = input.local_state.Cast<FanOutLocalSourceState>();

	// 1. Try to grab a chunk from the buffer
	if (TryConsume(gstate, lstate, chunk)) {
		return SourceResultType::HAVE_MORE_OUTPUT;
	}

	// 2. Buffer empty — check if source is exhausted
	if (gstate.exhausted.load()) {
		return SourceResultType::FINISHED;
	}

	// 3. Try to become the producer
	bool expected = false;
	if (gstate.producing.compare_exchange_strong(expected, true)) {
		// We're the producer — fill the buffer
		Produce(gstate, *this, context, input.interrupt_state);

		// Try to consume one for ourselves
		if (TryConsume(gstate, lstate, chunk)) {
			return SourceResultType::HAVE_MORE_OUTPUT;
		}
		return gstate.exhausted.load() ? SourceResultType::FINISHED : SourceResultType::HAVE_MORE_OUTPUT;
	}

	// 4. Someone else is producing — block until a slot is available
	annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
	// Double-check: a slot might have appeared while we were getting the lock
	if (TryConsume(gstate, lstate, chunk)) {
		return SourceResultType::HAVE_MORE_OUTPUT;
	}
	if (gstate.exhausted.load()) {
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
