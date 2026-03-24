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

	//! Chunk map: batch_id → chunk. Producer inserts, consumer erases.
	mutex map_lock;
	unordered_map<idx_t, unique_ptr<DataChunk>> chunks;
	idx_t next_read = 0;  //! Next batch_id to consume
	idx_t next_write = 0; //! Next batch_id to produce (only producer touches)

	//! Producer coordination
	atomic<bool> producing {false};
	atomic<bool> exhausted {false};

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
// TryConsume — brief lock to claim and take a chunk
//===--------------------------------------------------------------------===//
static bool TryConsume(FanOutGlobalSourceState &gstate, FanOutLocalSourceState &lstate, DataChunk &chunk) {
	lock_guard<mutex> lock(gstate.map_lock);
	auto it = gstate.chunks.find(gstate.next_read);
	if (it == gstate.chunks.end()) {
		return false;
	}
	chunk.Move(*it->second);
	lstate.current_batch = gstate.next_read;
	gstate.chunks.erase(it);
	gstate.next_read++;
	return true;
}

//===--------------------------------------------------------------------===//
// Produce — GetData without lock, brief lock to insert into map
//===--------------------------------------------------------------------===//
static void Produce(FanOutGlobalSourceState &gstate, const PhysicalFanOut &op, ExecutionContext &context,
                    InterruptState &interrupt_state) {
	for (idx_t i = 0; i < BATCH_SIZE; i++) {
		// Check map size under lock
		{
			lock_guard<mutex> lock(gstate.map_lock);
			if (gstate.chunks.size() >= BATCH_SIZE) {
				break; // buffer full
			}
		}

		// GetData without any lock — only one producer
		auto temp = make_uniq<DataChunk>();
		temp->Initialize(Allocator::DefaultAllocator(), gstate.child_types);

		OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, interrupt_state};
		auto result = op.child_source.get().GetData(context, *temp, child_input);

		if (result == SourceResultType::FINISHED || temp->size() == 0) {
			gstate.exhausted.store(true, std::memory_order_release);
			annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
			gstate.blocked_state.UnblockTasks();
			break;
		}

		// Brief lock to insert
		{
			lock_guard<mutex> lock(gstate.map_lock);
			gstate.chunks[gstate.next_write] = std::move(temp);
			gstate.next_write++;
		}

		// Wake consumers
		{
			annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
			gstate.blocked_state.UnblockTasks();
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
