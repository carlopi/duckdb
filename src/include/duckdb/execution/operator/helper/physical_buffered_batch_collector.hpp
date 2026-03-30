//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/helper/physical_buffered_batch_collector.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

//! --- Future: Lazy Batch Sink Protocol ---
//! Current bottleneck: sink->NextBatch is called per batch index change (48K times
//! for 100M rows / 2048 per chunk). Each call takes locks in the batch collector
//! (CompleteBatch + UpdateMinBatchIndex) and pipeline executor (UpdateBatchIndex).
//!
//! Proposed lazy protocol:
//!   Sink: append-only, receives chunks with batch indices, no completion tracking.
//!         Just stores chunks indexed by batch — lock-free or one lock per N chunks.
//!   NextBatch: no-op or updates local partition_info only (no locks).
//!   Scan (consumer-driven): when the client pulls data, compute which batches are
//!         complete by comparing batch indices against the pipeline's min_batch_index.
//!         Move completed batches to read_queue at scan time.
//!
//! This inverts control: producer pushes data without synchronization overhead,
//! consumer resolves ordering at read time. The sink becomes a simple concurrent
//! append buffer. Ordering is deferred to when it's actually needed.
//!
//! Expected impact: eliminates per-chunk lock overhead (48K→0 locks in hot path).
//! The 0.67s regression on SELECT * WHERE with batch collector would drop to ~0.1s.
//!
//! Alternative: reuse FanOut's FanIn buffer as the batch collector.
//! The batch collector is fundamentally a FanIn: M threads push chunks with batch
//! indices, ordered output is consumed by Scan. FanOut's buffer (with the accessing
//! counter + completed tracking) already handles concurrent writers lock-free.
//! The batch collector would become a FanIn — same buffer, same protocol, chunks
//! flow in from multiple threads, Scan drains in batch order.
//! This unifies the architecture: FanOut for source→parallel, FanIn for
//! parallel→ordered-output. Same buffer, both directions.
//!
//! Simplest approach: place FanIn in front of the batch collector as a pipeline-
//! transparent buffer. Multiple threads push to FanIn lock-free. A single drain
//! thread feeds the batch collector sequentially — eliminating all lock contention
//! in the collector without changing its code. The collector sees a single-threaded
//! stream of ordered chunks.

#include "duckdb/execution/operator/helper/physical_result_collector.hpp"
#include "duckdb/common/queue.hpp"

namespace duckdb {

class BufferedBatchCollectorLocalState : public LocalSinkState {
public:
	BufferedBatchCollectorLocalState();

public:
	idx_t current_batch = 0;
	//! Locally accumulated chunks + batch completions, flushed in bulk
	vector<pair<idx_t, unique_ptr<DataChunk>>> pending_chunks;
	vector<idx_t> pending_completions;
	idx_t pending_count = 0;
	static constexpr idx_t FLUSH_THRESHOLD = 64;
};

class PhysicalBufferedBatchCollector : public PhysicalResultCollector {
public:
	PhysicalBufferedBatchCollector(PhysicalPlan &physical_plan, PreparedStatementData &data);

public:
	unique_ptr<QueryResult> GetResult(GlobalSinkState &state) const override;

public:
	// Sink interface
	SinkResultType Sink(ExecutionContext &context, DataChunk &chunk, OperatorSinkInput &input) const override;
	SinkCombineResultType Combine(ExecutionContext &context, OperatorSinkCombineInput &input) const override;
	SinkNextBatchType NextBatch(ExecutionContext &context, OperatorSinkNextBatchInput &input) const override;

	unique_ptr<LocalSinkState> GetLocalSinkState(ExecutionContext &context) const override;
	unique_ptr<GlobalSinkState> GetGlobalSinkState(ClientContext &context) const override;

	OperatorPartitionInfo RequiredPartitionInfo() const override {
		return OperatorPartitionInfo::BatchIndex();
	}

	bool ParallelSink() const override {
		return true;
	}

	bool IsStreaming() const override {
		return true;
	}
};

} // namespace duckdb
