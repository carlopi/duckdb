//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/enums/source_chunks_lifetime.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/constants.hpp"

namespace duckdb {

//! Describes how long the data referenced by a DataChunk remains valid.
//! Used by operators that buffer chunks across boundaries (e.g. PhysicalFanOut,
//! CachingPhysicalOperator) to decide whether to deep-copy or just reference.
//!
//! Propagation through chunk operations:
//!   Initialize         -> OWNED (fresh buffers allocated by this chunk)
//!   Reset              -> unchanged
//!   Reference(other)   -> PIPELINE if other is PIPELINE, else TRANSIENT
//!   Slice(other, ...)  -> PIPELINE if other is PIPELINE, else TRANSIENT
//!   Move(other)        -> PIPELINE if other is PIPELINE, else TRANSIENT
//!   Copy(other)        -> other becomes OWNED (we deep-copied into it)
//!   Append(other)      -> this stays OWNED (we deep-copied from other into us)
//!
//! Note: Reference/Move downgrade OWNED -> TRANSIENT because once data flows
//! through a Reference/Move, we no longer know whether the upstream owner will
//! continue to exist. Only explicit PIPELINE contracts carry through.
enum class ChunkLifetime : uint8_t {
	//! Valid only until the next operation on the producer. Consumers buffering
	//! across calls must deep-copy.
	TRANSIENT,
	//! Valid for the duration of the pipeline execution. Consumers can hold
	//! references across GetData/Execute calls without copying, but must not
	//! hold beyond the pipeline's end.
	PIPELINE,
	//! Chunk owns its buffers — valid until the chunk is destroyed. Freshly
	//! Initialize()d chunks start at OWNED. Append/Copy produce OWNED output.
	OWNED
};

} // namespace duckdb
