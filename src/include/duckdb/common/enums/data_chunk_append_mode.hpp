//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/enums/data_chunk_append_mode.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/constants.hpp"

namespace duckdb {

//! Controls what happens to the input DataChunk on DataChunk::Append.
enum class DataChunkAppendMode : uint8_t {
	//! Value-copy the input; input is unchanged (default — matches the
	//! Append(const DataChunk &) overload).
	COPY,
	//! Value-copy the input and Reset it afterwards (cardinality zeroed,
	//! vectors still allocated — input is reusable for the next iteration).
	//! Fast path: when this is empty and input is OWNED, swap vectors so this
	//! takes input's data and input keeps this's empty-but-initialized vectors
	//! — zero-copy, input still in Reset-like state.
	COPY_AND_RESET,
	//! Consume input: when this is empty and input is OWNED, Move for zero-copy
	//! ownership transfer; otherwise value-copy followed by Destroy. After the
	//! call, input has no vectors — caller must reinitialize if reusing.
	CONSUME
};

} // namespace duckdb
