//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/main/base_client_context.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/constants.hpp"

namespace duckdb {
class Allocator;
class Vector;

//! BaseClientContext is the abstract interface used by rendering and casting code.
//! ClientContext (full engine) and (later) ShallowClientContext (stub, no engine) both implement it.
class BaseClientContext {
public:
	virtual ~BaseClientContext() = default;

	//! Whether execution has been interrupted
	virtual bool IsInterrupted() const = 0;

	//! Cast source vector into result vector
	virtual void Cast(Vector &source, Vector &result, idx_t count, bool strict = false) = 0;

	//! Get the allocator for temporary data
	virtual Allocator &GetAllocator() = 0;
};

} // namespace duckdb
