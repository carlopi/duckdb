//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/types/isnan.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/winapi.hpp"

namespace duckdb {

DUCKDB_API bool FloatIsNan(float input);
DUCKDB_API bool FloatIsNan(double input);

} // namespace duckdb
