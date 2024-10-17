//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/function/function_set.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

namespace duckdb {

// This is the odd one, but currently used in
//   extension/core_functions/include/core_functions/aggregate/nested_functions.hpp
class LogicalType;

class ScalarFunction;
class AggregateFunction;
class TableFunction;
class PragmaFunction;
class ScalarFunctionSet;
class AggregateFunctionSet;
class TableFunctionSet;
class PragmaFunctionSet;

} // namespace duckdb
