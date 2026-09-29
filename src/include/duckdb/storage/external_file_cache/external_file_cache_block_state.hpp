//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/external_file_cache/external_file_cache_block_state.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/typedefs.hpp"

namespace duckdb {

enum class CacheBlockState : uint8_t {
	// Initial state.
	EMPTY,
	// A thread is actively performing I/O.
	LOADING,
	// Data is already available in block_handle.
	LOADED,
	// I/O failed.
	IO_ERROR,
	// Merged into a block of the same generation that covers its whole range, and removed from the cache.
	SUPERSEDED
};

} // namespace duckdb
