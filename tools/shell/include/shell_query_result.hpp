//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_query_result.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/main/query_result.hpp"
#include "shell_column_data_collection.hpp"

namespace duckdb_shell {
using duckdb::idx_t;
using duckdb::unique_ptr;

//! ShellQueryResult is an abstract interface for a query result (streaming or materialized).
class ShellQueryResult {
public:
	virtual ~ShellQueryResult() = default;

	//! Error handling
	virtual bool HasError() const = 0;
	virtual const duckdb::string &GetError() const = 0;

	//! Properties
	virtual duckdb::StatementReturnType GetReturnType() const = 0;
	virtual duckdb::QueryResultType GetResultType() const = 0;
	virtual idx_t ColumnCount() const = 0;

	//! Column metadata
	virtual const duckdb::vector<duckdb::string> &Names() const = 0;
	virtual const duckdb::vector<duckdb::LogicalType> &Types() const = 0;

	//! Data access
	virtual duckdb::unique_ptr<duckdb::DataChunk> Fetch() = 0;

	//! Row iteration — for `for (auto &row : *result)` patterns
	using iterator = duckdb::QueryResult::iterator;
	virtual iterator begin() = 0;
	virtual iterator end() = 0;
};

//! ShellMaterializedQueryResult is an abstract interface for a materialized query result.
class ShellMaterializedQueryResult : public ShellQueryResult {
public:
	virtual idx_t RowCount() const = 0;
	virtual ShellColumnDataCollection Collection() = 0;
};

} // namespace duckdb_shell
