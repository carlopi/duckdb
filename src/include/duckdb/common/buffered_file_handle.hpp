//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/buffered_file_handle.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once
#include "duckdb/common/file_system.hpp"

namespace duckdb {
struct FileHandle;

struct BufferedFileHandle : public WrappedFileHandle {
	DUCKDB_API BufferedFileHandle(unique_ptr<FileHandle> inner_handle, size_t start, size_t end, Allocator & allocator);
	DUCKDB_API  ~BufferedFileHandle();

	DUCKDB_API void Read(void *buffer, idx_t nr_bytes, idx_t location) override;
	DUCKDB_API void Close() override {
		GetInner().Close();
	}
	DUCKDB_API void Truncate(int64_t new_size) override {
		buffered.Reset();
		start = 0;
		end = 0;
		GetInner().Truncate(new_size);
	
}
	DUCKDB_API int64_t Write(void *buffer, idx_t nr_bytes) override {
		buffered.Reset();
		start = 0;
		end = 0;
		return GetInner().Write(buffer, nr_bytes);
	}
	DUCKDB_API void Write(void *buffer, idx_t nr_bytes, idx_t location) override {
		buffered.Reset();
		start = 0;
		end = 0;
		GetInner().Write(buffer, nr_bytes, location);
	}
	size_t start;
	size_t end;
	AllocatedData buffered;
};

}; // namespace duckdb
