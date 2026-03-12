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
#include "duckdb/main/materialized_query_result.hpp"
#include "shell_column_data_collection.hpp"

namespace duckdb_shell {
using duckdb::idx_t;
using duckdb::unique_ptr;

//! ShellQueryResult wraps a duckdb::QueryResult (streaming or materialized).
class ShellQueryResult {
public:
	explicit ShellQueryResult(duckdb::unique_ptr<duckdb::QueryResult> result);
	~ShellQueryResult();

	//! Error handling
	bool HasError() const;
	const duckdb::string &GetError() const;

	//! Properties
	duckdb::StatementReturnType GetReturnType() const;
	duckdb::QueryResultType GetResultType() const;
	idx_t ColumnCount() const;

	//! Column metadata
	const duckdb::vector<duckdb::string> &Names() const;
	const duckdb::vector<duckdb::LogicalType> &Types() const;

	//! Data access
	duckdb::unique_ptr<duckdb::DataChunk> Fetch();

	//! Row iteration — for `for (auto &row : *result)` patterns
	auto begin() -> decltype(std::declval<duckdb::QueryResult>().begin()) {
		return result->begin();
	}
	auto end() -> decltype(std::declval<duckdb::QueryResult>().end()) {
		return result->end();
	}

	//! Engine access — needed by RenderingQueryResult and renderers
	duckdb::QueryResult &GetResult();

	//! Extract the underlying MaterializedQueryResult, transferring ownership.
	//! Only valid if GetResultType() == MATERIALIZED_RESULT.
	//! After calling this, the ShellQueryResult is empty.
	duckdb::unique_ptr<duckdb::MaterializedQueryResult> TakeMaterialized();

protected:
	duckdb::unique_ptr<duckdb::QueryResult> result;
};

//! ShellMaterializedQueryResult wraps a duckdb::MaterializedQueryResult.
class ShellMaterializedQueryResult {
public:
	explicit ShellMaterializedQueryResult(duckdb::unique_ptr<duckdb::MaterializedQueryResult> result);
	~ShellMaterializedQueryResult();

	//! Error handling
	bool HasError() const;
	const duckdb::string &GetError() const;

	//! Properties
	duckdb::StatementReturnType GetReturnType() const;

	//! Column metadata
	const duckdb::vector<duckdb::string> &Names() const;
	const duckdb::vector<duckdb::LogicalType> &Types() const;
	idx_t ColumnCount() const;

	//! Materialized-specific
	idx_t RowCount() const;
	ShellColumnDataCollection Collection();

	//! Row iteration
	auto begin() -> decltype(std::declval<duckdb::MaterializedQueryResult>().begin()) {
		return result->begin();
	}
	auto end() -> decltype(std::declval<duckdb::MaterializedQueryResult>().end()) {
		return result->end();
	}

	//! Engine access
	duckdb::MaterializedQueryResult &GetResult();

private:
	duckdb::unique_ptr<duckdb::MaterializedQueryResult> result;
};

} // namespace duckdb_shell
