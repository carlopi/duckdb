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
//!   Move(other)        -> other.lifetime (ownership transferred, not shared)
//!   Copy(other)        -> other becomes OWNED (we deep-copied into it)
//!   Append(other)      -> requires this is OWNED or PIPELINE; preserves
//!                         lifetime (writes values into this's own buffers)
//!
//! Note: Reference downgrades OWNED -> TRANSIENT because referencing creates
//! a second reader for the same buffer. Without a per-VectorBuffer ownership
//! guarantee, we can't tell if the underlying memory is kept alive by the
//! shared_ptr refcount. Move, in contrast, transfers ownership — no other
//! reader exists after the move — so the lifetime carries through unchanged.
//!
//! TODO: consider adding a NOT_INITIALIZED / NEEDS_INITIALIZE state (or
//! similar) to track chunks with no valid vectors — e.g. freshly default-
//! constructed, post-Destroy, or post-Move source chunks. Would let consumers
//! assert precondition ("this chunk has data to operate on") rather than
//! relying on implicit invariants around Destroy/Move usage.
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
