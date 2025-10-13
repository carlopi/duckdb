//===----------------------------------------------------------------------===//
//                         DuckDB
//
// thrift_tools.hpp
//
//
//===----------------------------------------------------------------------===/

#pragma once

#include <list>
#include "thrift/protocol/TCompactProtocol.h"
#include "thrift/transport/TBufferTransports.h"

#include "duckdb.hpp"
#include "duckdb/storage/caching_file_system.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/allocator.hpp"

#include "duckdb/parallel/task_executor.hpp"
#include "duckdb/parallel/executor_task.hpp"
#include <chrono>
#include <thread>

#include <iostream>

namespace duckdb {

// A ReadHead for prefetching data in a specific range
struct ReadHead {
	ReadHead(idx_t location, uint64_t size) : location(location), size(size) {};
	// Hint info
	idx_t location;
	uint64_t size;

	// Current info
	BufferHandle buffer_handle;
	data_ptr_t buffer_ptr;
	bool data_isset = false;

	idx_t GetEnd() const {
		return size + location;
	}
};

// Comparator for ReadHeads that are either overlapping, adjacent, or within ALLOW_GAP bytes from each other
struct ReadHeadComparator {
	static constexpr uint64_t ALLOW_GAP = 1 << 14; // 16 KiB
	bool operator()(const ReadHead *a, const ReadHead *b) const {
		auto a_start = a->location;
		auto a_end = a->location + a->size;
		auto b_start = b->location;

		if (a_end <= NumericLimits<idx_t>::Maximum() - ALLOW_GAP) {
			a_end += ALLOW_GAP;
		}

		return a_start < b_start && a_end < b_start;
	}
};

// Two-step read ahead buffer
// 1: register all ranges that will be read, merging ranges that are consecutive
// 2: prefetch all registered ranges
struct ReadAheadBuffer {
	explicit ReadAheadBuffer(CachingFileHandle &file_handle_p) : file_handle(file_handle_p) {
	}

	// The list of read heads
	std::list<ReadHead> read_heads;
	// Set for merging consecutive ranges
	std::set<ReadHead *, ReadHeadComparator> merge_set;

	CachingFileHandle &file_handle;

	idx_t total_size = 0;

	// Add a read head to the prefetching list
	void AddReadHead(idx_t pos, uint64_t len, bool merge_buffers = true) {
		// Attempt to merge with existing
		if (merge_buffers) {
			ReadHead new_read_head {pos, len};
			auto lookup_set = merge_set.find(&new_read_head);
			if (lookup_set != merge_set.end()) {
				auto existing_head = *lookup_set;
				auto new_start = MinValue<idx_t>(existing_head->location, new_read_head.location);
				auto new_length = MaxValue<idx_t>(existing_head->GetEnd(), new_read_head.GetEnd()) - new_start;
				existing_head->location = new_start;
				existing_head->size = new_length;
				return;
			}
		}

		read_heads.emplace_front(ReadHead(pos, len));
		total_size += len;
		auto &read_head = read_heads.front();

		if (merge_buffers) {
			merge_set.insert(&read_head);
		}

		if (read_head.GetEnd() > file_handle.GetFileSize()) {
			throw std::runtime_error("Prefetch registered for bytes outside file: " + file_handle.GetPath() +
			                         ", attempted range: [" + std::to_string(pos) + ", " +
			                         std::to_string(read_head.GetEnd()) +
			                         "), file size: " + std::to_string(file_handle.GetFileSize()));
		}
	}

	// Returns the relevant read head
	ReadHead *GetReadHead(idx_t pos) {
		for (auto &read_head : read_heads) {
			if (pos >= read_head.location && pos < read_head.GetEnd()) {
				return &read_head;
			}
		}
		return nullptr;
	}

	// Prefetch all read heads
	void Prefetch() {
		for (auto &read_head : read_heads) {
			if (read_head.GetEnd() > file_handle.GetFileSize()) {
				throw std::runtime_error("Prefetch registered requested for bytes outside file");
			}
			read_head.buffer_handle = file_handle.Read(read_head.buffer_ptr, read_head.size, read_head.location);
			D_ASSERT(read_head.buffer_handle.IsValid());
			read_head.data_isset = true;
		}
	}

	struct LambdaState {
		BufferHandle &buffer_handle;
		CachingFileHandle &file_handle;
		data_ptr_t &ptr;
		idx_t size;
		idx_t location;
		bool &data_isset;
	};
	static void compute(void *in) {
		std::cout << "compute!!!\n";
		LambdaState *state = (LambdaState *)in;
		state->buffer_handle = state->file_handle.Read(state->ptr, state->size, state->location);
		D_ASSERT(state->buffer_handle.IsValid());
		state->data_isset = true;
	}
	static void cleanup(void *state) {
		std::cout << "cleanup!!!\n";
		delete (LambdaState *)state;
	}
	// Prefetch all read heads
	unique_ptr<PromiseHolder> Prefetch(InterruptState &state) {
		if (read_heads.size() > 1) {
			Prefetch();
			return nullptr;
		} else if (read_heads.size() == 0) {
			return nullptr;
		} else {
			auto &read_head = *read_heads.begin();
			if (read_head.GetEnd() > file_handle.GetFileSize()) {
				throw std::runtime_error("Prefetch registered requested for bytes outside file");
			}

			LambdaState *in = new LambdaState({read_head.buffer_handle, file_handle, read_head.buffer_ptr,
			                                   read_head.size, read_head.location, read_head.data_isset});

			/*Promise promise;
			promise.promise_state = in;
			promise.compute_callback = compute;
			promise.compute_cleanup = cleanup;*/

			unique_ptr<PromiseHolder> promise_holder = make_uniq<PromiseHolder>();
			promise_holder->v.push_back(make_uniq<Promise>());
			promise_holder->v.back()->promise_state = in;
			promise_holder->v.back()->compute_callback = compute;
			promise_holder->v.back()->compute_cleanup = cleanup;

			std::cout << "yoo\t" << read_head.size << " at " << read_head.location << "\n";
			return std::move(promise_holder);
			// return

			//	return make_uniq<IOToBeScheduledTask>(read_head.buffer_handle, file_handle, read_head.buffer_ptr,
			//	                                      read_head.size, read_head.location, read_head.data_isset);
		}
	}
};

class ThriftFileTransport : public duckdb_apache::thrift::transport::TVirtualTransport<ThriftFileTransport> {
public:
	static constexpr uint64_t PREFETCH_FALLBACK_BUFFERSIZE = 1000000;

	ThriftFileTransport(CachingFileHandle &file_handle_p, bool prefetch_mode_p)
	    : file_handle(file_handle_p), location(0), size(file_handle.GetFileSize()),
	      ra_buffer(ReadAheadBuffer(file_handle)), prefetch_mode(prefetch_mode_p) {
	}

	uint32_t read(uint8_t *buf, uint32_t len) {
		auto prefetch_buffer = ra_buffer.GetReadHead(location);
		if (prefetch_buffer != nullptr && location - prefetch_buffer->location + len <= prefetch_buffer->size) {
			D_ASSERT(location - prefetch_buffer->location + len <= prefetch_buffer->size);

			if (!prefetch_buffer->data_isset) {
				prefetch_buffer->buffer_handle =
				    file_handle.Read(prefetch_buffer->buffer_ptr, prefetch_buffer->size, prefetch_buffer->location);
				D_ASSERT(prefetch_buffer->buffer_handle.IsValid());
				prefetch_buffer->data_isset = true;
			}
			D_ASSERT(prefetch_buffer->buffer_handle.IsValid());
			memcpy(buf, prefetch_buffer->buffer_ptr + location - prefetch_buffer->location, len);
		} else if (prefetch_mode && len < PREFETCH_FALLBACK_BUFFERSIZE && len > 0) {
			Prefetch(location, MinValue<uint64_t>(PREFETCH_FALLBACK_BUFFERSIZE, file_handle.GetFileSize() - location));
			auto prefetch_buffer_fallback = ra_buffer.GetReadHead(location);
			D_ASSERT(location - prefetch_buffer_fallback->location + len <= prefetch_buffer_fallback->size);
			memcpy(buf, prefetch_buffer_fallback->buffer_ptr + location - prefetch_buffer_fallback->location, len);
		} else {
			// No prefetch, do a regular (non-caching) read
			file_handle.GetFileHandle().Read(context, buf, len, location);
		}

		location += len;
		return len;
	}

	// Prefetch a single buffer
	void Prefetch(idx_t pos, uint64_t len) {
		RegisterPrefetch(pos, len, false);
		FinalizeRegistration();
		PrefetchRegistered();
	}

	// Prefetch a single buffer
	unique_ptr<PromiseHolder> Prefetch(idx_t pos, uint64_t len, InterruptState &interrupt_state) {
		RegisterPrefetch(pos, len, false);
		FinalizeRegistration();
		return PrefetchRegistered(interrupt_state);
	}

	// Register a buffer for prefixing
	void RegisterPrefetch(idx_t pos, uint64_t len, bool can_merge = true) {
		ra_buffer.AddReadHead(pos, len, can_merge);
	}

	// Prevents any further merges, should be called before PrefetchRegistered
	void FinalizeRegistration() {
		ra_buffer.merge_set.clear();
	}

	// Prefetch all previously registered ranges
	void PrefetchRegistered() {
		ra_buffer.Prefetch();
	}
	unique_ptr<PromiseHolder> PrefetchRegistered(InterruptState &interrupt_state) {
		return ra_buffer.Prefetch(interrupt_state);
	}

	void ClearPrefetch() {
		ra_buffer.read_heads.clear();
		ra_buffer.merge_set.clear();
	}

	void Skip(idx_t skip_count) {
		location += skip_count;
	}

	bool HasPrefetch() const {
		return !ra_buffer.read_heads.empty() || !ra_buffer.merge_set.empty();
	}

	void SetLocation(idx_t location_p) {
		location = location_p;
	}

	idx_t GetLocation() const {
		return location;
	}

	optional_ptr<ReadHead> GetReadHead(idx_t pos) {
		return ra_buffer.GetReadHead(pos);
	}

	idx_t GetSize() const {
		return size;
	}

private:
	QueryContext context;

	CachingFileHandle &file_handle;
	idx_t location;
	idx_t size;

	// Multi-buffer prefetch
	ReadAheadBuffer ra_buffer;

	// Whether the prefetch mode is enabled. In this mode the DirectIO flag of the handle will be set and the parquet
	// reader will manage the read buffering.
	bool prefetch_mode;
};

} // namespace duckdb
