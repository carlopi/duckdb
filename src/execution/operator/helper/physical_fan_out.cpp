#include "duckdb/execution/operator/helper/physical_fan_out.hpp"
#include "duckdb/execution/execution_context.hpp"
#include "duckdb/parallel/interrupt.hpp"
#include "duckdb/parallel/task_scheduler.hpp"

namespace duckdb {

PhysicalFanOut::PhysicalFanOut(PhysicalPlan &plan, PhysicalOperator &child_source_p, idx_t estimated_cardinality)
    : PhysicalOperator(plan, PhysicalOperatorType::FAN_OUT, child_source_p.types, estimated_cardinality),
      child_source(child_source_p) {
}

//===--------------------------------------------------------------------===//
// State
//===--------------------------------------------------------------------===//
static constexpr idx_t FAN_OUT_MAX_BATCH_SIZE = 64;
static constexpr idx_t FAN_OUT_MAX_BATCH_BYTES = 2ULL * 1024 * 1024; // 2MB per local buffer

class FanOutGlobalSourceState : public GlobalSourceState {
public:
	FanOutGlobalSourceState(ClientContext &context, const PhysicalFanOut &op) {
		child_global = op.child_source.get().GetGlobalSourceState(context);
		auto num_threads = TaskScheduler::GetScheduler(context).NumberOfThreads();
		max_threads = MaxValue<idx_t>(num_threads - 1, 1);
	}

	unique_ptr<GlobalSourceState> child_global;
	unique_ptr<LocalSourceState> child_local;
	bool child_local_initialized = false;

	//! Producer lock — only one thread fetches at a time
	mutex producer_lock;
	//! Global batch counter
	idx_t next_batch = 0;
	//! Current batch size — ramps up exponentially: 1, 1, 2, 4, 8, ... up to FAN_OUT_MAX_BATCH_SIZE
	//! (ramp up will be a power of 2 total)
	idx_t current_batch_size = 1;
	//! Whether the child source is exhausted
	atomic<bool> exhausted {false};
	//! Whether the child source is blocked (e.g. async task in flight)
	atomic<bool> child_blocked {false};

	//! Blocked threads waiting to become producer
	mutex blocked_lock;
	vector<InterruptState> blocked_threads;

	idx_t MaxThreads() override {
		return MaxValue<idx_t>(max_threads, 1);
	}
	idx_t max_threads;
};

class FanOutLocalSourceState : public LocalSourceState {
public:
	//! Pre-allocated chunk buffer — initialized once, reused across batches
	DataChunk chunks[FAN_OUT_MAX_BATCH_SIZE];
	idx_t chunk_count = 0;
	idx_t chunk_idx = 0;
	idx_t chunks_initialized = 0;
	//! Batch index assigned to this local buffer
	idx_t batch_index = 0;
	//! If true, BLOCK when local buffer is exhausted (child was blocked during fetch)
	bool block_after_drain = false;
	//! If true, this thread was the producer that got blocked by the child — it should resume producing
	bool resume_as_producer = false;
};

//===--------------------------------------------------------------------===//
// Init
//===--------------------------------------------------------------------===//
unique_ptr<GlobalSourceState> PhysicalFanOut::GetGlobalSourceState(ClientContext &context) const {
	return make_uniq_base<GlobalSourceState, FanOutGlobalSourceState>(context, *this);
}

unique_ptr<LocalSourceState> PhysicalFanOut::GetLocalSourceState(ExecutionContext &context,
                                                                 GlobalSourceState &gstate) const {
	auto &fan_out_gstate = gstate.Cast<FanOutGlobalSourceState>();
	auto result = make_uniq<FanOutLocalSourceState>();
	// Single shared child local state
	lock_guard<mutex> lock(fan_out_gstate.producer_lock);
	if (!fan_out_gstate.child_local_initialized) {
		fan_out_gstate.child_local = child_source.get().GetLocalSourceState(context, *fan_out_gstate.child_global);
		fan_out_gstate.child_local_initialized = true;
	}
	return std::move(result);
}

//===--------------------------------------------------------------------===//
// GetData
//===--------------------------------------------------------------------===//
SourceResultType PhysicalFanOut::GetDataInternal(ExecutionContext &context, DataChunk &chunk,
                                                 OperatorSourceInput &input) const {
	auto &gstate = input.global_state.Cast<FanOutGlobalSourceState>();
	auto &lstate = input.local_state.Cast<FanOutLocalSourceState>();

	// Step 1: if we have local chunks, serve them by reference (no copy)
	if (lstate.chunk_idx < lstate.chunk_count) {
		chunk.Reference(lstate.chunks[lstate.chunk_idx]);
		lstate.chunk_idx++;
		if (lstate.chunk_idx >= lstate.chunk_count) {
			lstate.chunk_count = 0;
			lstate.chunk_idx = 0;
		}
		return SourceResultType::HAVE_MORE_OUTPUT;
	}

	// Step 1b: if we need to block after draining (child was blocked during our fetch), do it now
	if (lstate.block_after_drain) {
		lstate.block_after_drain = false;
		lstate.resume_as_producer = true;
		return SourceResultType::BLOCKED;
	}

	// Step 2: check if exhausted
	if (gstate.exhausted.load(std::memory_order_acquire)) {
		return SourceResultType::FINISHED;
	}

	// Step 3: try to become the producer
	if (gstate.producer_lock.try_lock()) {
		// Use unique_lock with adopt_lock so producer_lock is released on exception or scope exit
		unique_lock<mutex> producer_guard(gstate.producer_lock, std::adopt_lock);

		// If child is blocked and we're not the resuming producer — don't produce
		if (gstate.child_blocked.load(std::memory_order_acquire) && !lstate.resume_as_producer) {
			// But if exhausted, don't block — let it through to discover FINISHED
			if (!gstate.exhausted.load(std::memory_order_acquire)) {
				producer_guard.unlock();
				lock_guard<mutex> lock(gstate.blocked_lock);
				gstate.blocked_threads.push_back(input.interrupt_state);
				return SourceResultType::BLOCKED;
			}
		}
		// Clear resume flag and child_blocked — we're taking over
		lstate.resume_as_producer = false;
		gstate.child_blocked.store(false, std::memory_order_release);
		// Re-check exhausted under lock
		if (gstate.exhausted.load(std::memory_order_acquire)) {
			producer_guard.unlock();
			// Wake all blocked threads so they can finish
			lock_guard<mutex> lock(gstate.blocked_lock);
			for (auto &blocked : gstate.blocked_threads) {
				blocked.Callback();
			}
			gstate.blocked_threads.clear();
			return SourceResultType::FINISHED;
		}

		// We are the producer — fetch N chunks into our local buffer
		// Batch size ramps up: 1, 1, 2, 4, 8, ... up to FAN_OUT_MAX_BATCH_SIZE
		auto batch_size = gstate.current_batch_size;
		if (gstate.current_batch_size < FAN_OUT_MAX_BATCH_SIZE) {
			if (gstate.next_batch == 0) {
				// First two rounds are size 1 to establish parallelism quickly
			} else {
				gstate.current_batch_size = MinValue(gstate.current_batch_size * 2, FAN_OUT_MAX_BATCH_SIZE);
			}
		}
		bool source_exhausted = false;
		bool source_blocked = false;
		idx_t batch_bytes = 0;
		// Initialize chunks up to batch_size (lazily, only what's needed)
		while (lstate.chunks_initialized < batch_size) {
			lstate.chunks[lstate.chunks_initialized].Initialize(Allocator::DefaultAllocator(),
			                                                    child_source.get().types);
			lstate.chunks_initialized++;
		}

		lstate.chunk_count = 0;
		for (idx_t i = 0;
		     i < batch_size && !source_exhausted && !source_blocked && batch_bytes < FAN_OUT_MAX_BATCH_BYTES; i++) {
			lstate.chunks[lstate.chunk_count].Reset();

			OperatorSourceInput child_input {*gstate.child_global, *gstate.child_local, input.interrupt_state};
			auto result = child_source.get().GetData(context, lstate.chunks[lstate.chunk_count], child_input);

			if (lstate.chunks[lstate.chunk_count].size() > 0) {
				batch_bytes += lstate.chunks[lstate.chunk_count].GetAllocationSize();
				lstate.chunk_count++;
			}

			if (result == SourceResultType::FINISHED) {
				source_exhausted = true;
			} else if (result == SourceResultType::BLOCKED) {
				source_blocked = true;
			}
		}

		// Assign batch index
		lstate.batch_index = gstate.next_batch++;
		lstate.chunk_idx = 0;

		if (source_blocked) {
			gstate.child_blocked.store(true, std::memory_order_release);
			if (lstate.chunk_count > 0) {
				// We have data to serve first, then block when drained
				lstate.block_after_drain = true;
			}
		} else {
			gstate.child_blocked.store(false, std::memory_order_release);
		}

		if (source_exhausted) {
			gstate.exhausted.store(true, std::memory_order_release);
		}

		producer_guard.unlock();

		// Wake blocked threads — but NOT if child source is blocked
		if (!source_blocked) {
			lock_guard<mutex> lock(gstate.blocked_lock);
			if (gstate.exhausted.load(std::memory_order_acquire)) {
				for (auto &blocked : gstate.blocked_threads) {
					blocked.Callback();
				}
				gstate.blocked_threads.clear();
			} else if (!gstate.blocked_threads.empty()) {
				gstate.blocked_threads.back().Callback();
				gstate.blocked_threads.pop_back();
			}
		}

		// If we got nothing, finish, block, or retry
		if (lstate.chunk_count == 0) {
			if (gstate.exhausted.load(std::memory_order_acquire)) {
				return SourceResultType::FINISHED;
			}
			if (source_blocked) {
				lstate.resume_as_producer = true;
				return SourceResultType::BLOCKED;
			}
			return SourceResultType::HAVE_MORE_OUTPUT;
		}

		// Start consuming our first chunk
		chunk.Reference(lstate.chunks[lstate.chunk_idx]);
		lstate.chunk_idx++;
		if (lstate.chunk_idx >= lstate.chunk_count) {
			lstate.chunk_count = 0;
			lstate.chunk_idx = 0;
		}
		return SourceResultType::HAVE_MORE_OUTPUT;
	}

	// Step 4: couldn't get lock — BLOCK until woken by producer
	{
		lock_guard<mutex> lock(gstate.blocked_lock);
		// Re-check after taking lock
		if (gstate.exhausted.load(std::memory_order_acquire)) {
			return SourceResultType::FINISHED;
		}
		gstate.blocked_threads.push_back(input.interrupt_state);
	}
	return SourceResultType::BLOCKED;
}

//===--------------------------------------------------------------------===//
// Partition data
//===--------------------------------------------------------------------===//
OperatorPartitionData PhysicalFanOut::GetPartitionData(ExecutionContext &context, DataChunk &chunk,
                                                       GlobalSourceState &gstate_p, LocalSourceState &lstate_p,
                                                       const OperatorPartitionInfo &partition_info) const {
	auto &lstate = lstate_p.Cast<FanOutLocalSourceState>();
	OperatorPartitionData result(lstate.batch_index);
	return result;
}

} // namespace duckdb
