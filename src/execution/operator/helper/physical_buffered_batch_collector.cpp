#include "duckdb/execution/operator/helper/physical_buffered_batch_collector.hpp"

#include "duckdb/common/types/batched_data_collection.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/materialized_query_result.hpp"
#include "duckdb/main/buffered_data/buffered_data.hpp"
#include "duckdb/main/buffered_data/batched_buffered_data.hpp"
#include "duckdb/main/stream_query_result.hpp"

namespace duckdb {

PhysicalBufferedBatchCollector::PhysicalBufferedBatchCollector(PhysicalPlan &physical_plan, PreparedStatementData &data)
    : PhysicalResultCollector(physical_plan, data) {
}

//===--------------------------------------------------------------------===//
// Sink
//===--------------------------------------------------------------------===//
class BufferedBatchCollectorGlobalState : public GlobalSinkState {
public:
	weak_ptr<ClientContext> context;
	shared_ptr<BufferedData> buffered_data;
};

BufferedBatchCollectorLocalState::BufferedBatchCollectorLocalState() {
}

SinkResultType PhysicalBufferedBatchCollector::Sink(ExecutionContext &context, DataChunk &chunk,
                                                    OperatorSinkInput &input) const {
	auto &gstate = input.global_state.Cast<BufferedBatchCollectorGlobalState>();
	auto &lstate = input.local_state.Cast<BufferedBatchCollectorLocalState>();

	lstate.current_batch = lstate.partition_info.batch_index.GetIndex();
	auto batch = lstate.partition_info.batch_index.GetIndex();

	auto &buffered_data = gstate.buffered_data->Cast<BatchedBufferedData>();

	if (buffered_data.ShouldBlockBatch(batch)) {
		// Flush pending before blocking
		if (!lstate.pending_chunks.empty()) {
			auto min_batch_index = lstate.partition_info.min_batch_index.GetIndex();
			buffered_data.AppendAndCompleteBatches(lstate.pending_chunks, lstate.pending_completions, min_batch_index);
			lstate.pending_completions.clear();
			lstate.pending_count = 0;
		}
		auto callback_state = input.interrupt_state;
		buffered_data.BlockSink(callback_state, batch);
		return SinkResultType::BLOCKED;
	}

	// Accumulate locally — no lock
	auto pending_chunk = make_uniq<DataChunk>();
	pending_chunk->Initialize(Allocator::DefaultAllocator(), chunk.GetTypes());
	chunk.Copy(*pending_chunk, 0);
	lstate.pending_chunks.emplace_back(batch, std::move(pending_chunk));

	return SinkResultType::NEED_MORE_INPUT;
}

SinkNextBatchType PhysicalBufferedBatchCollector::NextBatch(ExecutionContext &context,
                                                            OperatorSinkNextBatchInput &input) const {
	auto &gstate = input.global_state.Cast<BufferedBatchCollectorGlobalState>();
	auto &lstate = input.local_state.Cast<BufferedBatchCollectorLocalState>();

	auto new_index = lstate.partition_info.batch_index.GetIndex();

	// Accumulate batch completion locally
	lstate.pending_completions.push_back(lstate.current_batch);
	lstate.pending_count++;
	lstate.current_batch = new_index;

	// Flush in bulk when threshold reached — one lock for all appends + completions
	if (lstate.pending_count >= BufferedBatchCollectorLocalState::FLUSH_THRESHOLD) {
		auto &buffered_data = gstate.buffered_data->Cast<BatchedBufferedData>();
		auto min_batch_index = lstate.partition_info.min_batch_index.GetIndex();
		buffered_data.AppendAndCompleteBatches(lstate.pending_chunks, lstate.pending_completions, min_batch_index);
		lstate.pending_completions.clear();
		lstate.pending_count = 0;
	}

	return SinkNextBatchType::READY;
}

SinkCombineResultType PhysicalBufferedBatchCollector::Combine(ExecutionContext &context,
                                                              OperatorSinkCombineInput &input) const {
	auto &gstate = input.global_state.Cast<BufferedBatchCollectorGlobalState>();
	auto &lstate = input.local_state.Cast<BufferedBatchCollectorLocalState>();

	auto &buffered_data = gstate.buffered_data->Cast<BatchedBufferedData>();

	// Flush remaining pending chunks + completions
	if (!lstate.pending_chunks.empty() || !lstate.pending_completions.empty()) {
		auto min_batch_index = lstate.partition_info.min_batch_index.GetIndex();
		buffered_data.AppendAndCompleteBatches(lstate.pending_chunks, lstate.pending_completions, min_batch_index);
		lstate.pending_completions.clear();
		lstate.pending_count = 0;
	}
	return SinkCombineResultType::FINISHED;
}

unique_ptr<LocalSinkState> PhysicalBufferedBatchCollector::GetLocalSinkState(ExecutionContext &context) const {
	auto state = make_uniq<BufferedBatchCollectorLocalState>();
	return std::move(state);
}

unique_ptr<GlobalSinkState> PhysicalBufferedBatchCollector::GetGlobalSinkState(ClientContext &context) const {
	auto state = make_uniq<BufferedBatchCollectorGlobalState>();
	state->context = context.shared_from_this();
	state->buffered_data = make_shared_ptr<BatchedBufferedData>(context);
	return std::move(state);
}

unique_ptr<QueryResult> PhysicalBufferedBatchCollector::GetResult(GlobalSinkState &state) const {
	auto &gstate = state.Cast<BufferedBatchCollectorGlobalState>();
	auto cc = gstate.context.lock();
	auto result = make_uniq<StreamQueryResult>(statement_type, properties, types, names, cc->GetClientProperties(),
	                                           gstate.buffered_data);
	return std::move(result);
}

} // namespace duckdb
