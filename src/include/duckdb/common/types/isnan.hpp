//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/types/isnan.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

namespace duckdb {

DUCKDB_API bool FloatIsNan(float input);
DUCKDB_API bool FloatIsNan(double input);

} // namespace duckdb
