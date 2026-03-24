#include "duckdb/execution/operator/helper/physical_fan_out.hpp"
#include "duckdb/execution/execution_context.hpp"
#include "duckdb/common/chrono.hpp"
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

//! Number of GetData calls on one thread before fan-out decides whether to stay parallel
//! Call 1: skip (initialization overhead), calls 2+3: measure source + downstream
static constexpr idx_t WARMUP_CALLS = 3;
//! First N calls are not timed (may include one-time initialization)
static constexpr idx_t WARMUP_SKIP = 1;

class FanOutGlobalSourceState : public GlobalSourceState {
public:
	FanOutGlobalSourceState(ClientContext &context, const PhysicalFanOut &op) {
		child_global = op.child_source.get().GetGlobalSourceState(context);
	}

	//! Serializes access to the child source
	mutex source_lock;
	//! Manages blocked tasks during warmup
	StateWithBlockableTasks blocked_state;
	//! Monotonically increasing batch index
	idx_t next_batch = 0;
	//! Whether the child source is exhausted
	bool exhausted = false;
	//! The child source's global state (single instance, accessed under lock)
	unique_ptr<GlobalSourceState> child_global;
	//! A single shared child local state (accessed under lock)
	unique_ptr<LocalSourceState> child_local;
	//! Whether child_local has been initialized
	bool child_local_initialized = false;

	//! --- Warmup ---
	//! Pointer to the local state of the warmup task (stable across thread rescheduling)
	const LocalSourceState *warmup_task = nullptr;
	bool warmup_done = false;
	//! After warmup: true if downstream is heavy enough to justify fan-out
	bool fan_out_enabled = false;

	//! --- Timing stats (updated under source_lock) ---
	double total_get_data_time = 0;
	double total_downstream_time = 0;
	idx_t get_data_calls = 0;
	idx_t downstream_samples = 0;

	idx_t MaxThreads() override {
		return NumericLimits<idx_t>::Maximum();
	}
};

class FanOutLocalSourceState : public LocalSourceState {
public:
	//! Batch index assigned to the most recent chunk from this thread
	idx_t current_batch = 0;
	//! Timestamp when this thread last returned from GetData
	time_point<steady_clock> last_return_time;
	//! Whether last_return_time is valid
	bool has_returned = false;
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
	lock_guard<mutex> lock(fan_gstate.source_lock);
	if (!fan_gstate.child_local_initialized) {
		fan_gstate.child_local =
		    child_source.get().GetLocalSourceState(context, *fan_gstate.child_global);
		fan_gstate.child_local_initialized = true;
	}
	return make_uniq<FanOutLocalSourceState>();
}

//===--------------------------------------------------------------------===//
// GetData
//===--------------------------------------------------------------------===//
SourceResultType PhysicalFanOut::GetDataInternal(ExecutionContext &context, DataChunk &chunk,
                                                  OperatorSourceInput &input) const {
	auto &gstate = input.global_state.Cast<FanOutGlobalSourceState>();
	auto &lstate = input.local_state.Cast<FanOutLocalSourceState>();

	lock_guard<mutex> lock(gstate.source_lock);

	// --- Warmup gating via BLOCKED ---
	if (!gstate.warmup_done) {
		if (!gstate.warmup_task) {
			// First task in — becomes the warmup task
			gstate.warmup_task = &lstate;
		} else if (gstate.warmup_task != &lstate) {
			// Not the warmup task — return BLOCKED, will be woken up later
			annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
			return gstate.blocked_state.BlockSource(input.interrupt_state);
		}
	}

	// --- After warmup: if fan-out not justified, only warmup task continues ---
	if (gstate.warmup_done && !gstate.fan_out_enabled) {
		if (gstate.warmup_task != &lstate) {
			return SourceResultType::FINISHED;
		}
	}

	// --- Measure downstream time (skip first call — initialization overhead) ---
	bool should_measure = gstate.get_data_calls >= WARMUP_SKIP;
	if (should_measure && lstate.has_returned) {
		auto now = steady_clock::now();
		double downstream_time = std::chrono::duration<double>(now - lstate.last_return_time).count();
		gstate.total_downstream_time += downstream_time;
		gstate.downstream_samples++;
	}

	if (gstate.exhausted) {
		return SourceResultType::FINISHED;
	}

	// --- Measure source GetData time (skip first call) ---
	auto get_data_start = steady_clock::now();

	OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, input.interrupt_state};
	auto result = child_source.get().GetData(context, chunk, child_input);

	if (should_measure) {
		double get_data_time = std::chrono::duration<double>(steady_clock::now() - get_data_start).count();
		gstate.total_get_data_time += get_data_time;
	}
	gstate.get_data_calls++;

	if (result == SourceResultType::FINISHED || chunk.size() == 0) {
		gstate.exhausted = true;
		if (!gstate.warmup_done) {
			gstate.warmup_done = true;
			annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
			gstate.blocked_state.UnblockTasks();
		}
		return SourceResultType::FINISHED;
	}

	// --- End warmup after WARMUP_CALLS ---
	if (!gstate.warmup_done && gstate.get_data_calls >= WARMUP_CALLS) {
		gstate.warmup_done = true;

		// Decision: is downstream work heavy enough to justify fan-out?
		idx_t measured_calls = gstate.get_data_calls - WARMUP_SKIP;
		double avg_get_data = measured_calls > 0
		                          ? gstate.total_get_data_time / static_cast<double>(measured_calls)
		                          : 0;
		double avg_downstream = gstate.downstream_samples > 0
		                            ? gstate.total_downstream_time / static_cast<double>(gstate.downstream_samples)
		                            : 0;

		gstate.fan_out_enabled = avg_downstream > avg_get_data * 10;

		// Wake blocked threads
		annotated_lock_guard<annotated_mutex> blocked_lock(gstate.blocked_state.lock);
		if (!gstate.fan_out_enabled) {
			gstate.blocked_state.PreventBlocking();
		}
		gstate.blocked_state.UnblockTasks();
	}

	// Stamp batch index
	lstate.current_batch = gstate.next_batch++;

	// Record return time for downstream measurement
	lstate.last_return_time = steady_clock::now();
	lstate.has_returned = true;

	return result;
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
