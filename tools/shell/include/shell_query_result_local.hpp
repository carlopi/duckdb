//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_query_result_local.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_query_result.hpp"
#include "shell_column_data_collection.hpp"
#include "duckdb/main/materialized_query_result.hpp"

namespace duckdb_shell {

//! ShellQueryResultLocal wraps a duckdb::QueryResult (streaming or materialized).
class ShellQueryResultLocal : public ShellQueryResult {
public:
	explicit ShellQueryResultLocal(duckdb::unique_ptr<duckdb::QueryResult> result);
	~ShellQueryResultLocal() override;

	//! Base interface
	bool HasError() const override;
	const duckdb::string &GetError() const override;
	duckdb::StatementReturnType GetReturnType() const override;
	duckdb::QueryResultType GetResultType() const override;
	idx_t ColumnCount() const override;
	const duckdb::vector<duckdb::string> &Names() const override;
	const duckdb::vector<duckdb::LogicalType> &Types() const override;
	duckdb::unique_ptr<duckdb::DataChunk> Fetch() override;

	//! Row iteration
	iterator begin() override;
	iterator end() override;

	//! Engine-only: raw result access
	duckdb::QueryResult &GetResult();

	//! Engine-only: extract the underlying MaterializedQueryResult, transferring ownership.
	//! Only valid if GetResultType() == MATERIALIZED_RESULT.
	//! After calling this, the ShellQueryResultLocal is empty.
	duckdb::unique_ptr<duckdb::MaterializedQueryResult> TakeMaterialized();

private:
	duckdb::unique_ptr<duckdb::QueryResult> result;
};

//! ShellMaterializedQueryResultLocal wraps a duckdb::MaterializedQueryResult.
class ShellMaterializedQueryResultLocal : public ShellMaterializedQueryResult {
public:
	explicit ShellMaterializedQueryResultLocal(duckdb::unique_ptr<duckdb::MaterializedQueryResult> result);
	~ShellMaterializedQueryResultLocal() override;

	//! Base interface
	bool HasError() const override;
	const duckdb::string &GetError() const override;
	duckdb::StatementReturnType GetReturnType() const override;
	duckdb::QueryResultType GetResultType() const override;
	idx_t ColumnCount() const override;
	const duckdb::vector<duckdb::string> &Names() const override;
	const duckdb::vector<duckdb::LogicalType> &Types() const override;
	duckdb::unique_ptr<duckdb::DataChunk> Fetch() override;

	//! Row iteration
	iterator begin() override;
	iterator end() override;

	//! Materialized-specific
	idx_t RowCount() const override;
	ShellColumnDataCollection Collection() override;

	//! Engine-only: raw result access
	duckdb::MaterializedQueryResult &GetMaterializedResult();

private:
	duckdb::unique_ptr<duckdb::MaterializedQueryResult> result;
};

} // namespace duckdb_shell
