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

struct BufferedFileHandle {
	DUCKDB_API BufferedFileHandle(FileHandle &inner_handle, size_t start, size_t end, Allocator & allocator);
	DUCKDB_API  ~BufferedFileHandle();

	DUCKDB_API void Read(void *buffer, idx_t nr_bytes, idx_t location);
	DUCKDB_API idx_t GetFileSize();
	DUCKDB_API string GetPath();

	FileHandle &inner_handle;
	size_t start;
	size_t end;
	AllocatedData buffered;
	Allocator & allocator;
};

}; // namespace duckdb
