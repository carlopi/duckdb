//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/enums/scan_vector_type.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/constants.hpp"

namespace duckdb {

enum class ScanVectorType : uint8_t { SCAN_ENTIRE_VECTOR, SCAN_FLAT_VECTOR };

} // namespace duckdb
