//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_query_result_wired.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_query_result.hpp"

namespace duckdb_shell {

//! Wire-mode stub for ShellQueryResult.
class ShellQueryResultWired : public ShellQueryResult {
public:
	ShellQueryResultWired();
	~ShellQueryResultWired() override;

	bool HasError() const override;
	const duckdb::string &GetError() const override;
	duckdb::StatementReturnType GetReturnType() const override;
	duckdb::QueryResultType GetResultType() const override;
	idx_t ColumnCount() const override;
	const duckdb::vector<duckdb::string> &Names() const override;
	const duckdb::vector<duckdb::LogicalType> &Types() const override;
	duckdb::unique_ptr<duckdb::DataChunk> Fetch() override;
	iterator begin() override;
	iterator end() override;
};

//! Wire-mode stub for ShellMaterializedQueryResult.
class ShellMaterializedQueryResultWired : public ShellMaterializedQueryResult {
public:
	ShellMaterializedQueryResultWired();
	~ShellMaterializedQueryResultWired() override;

	bool HasError() const override;
	const duckdb::string &GetError() const override;
	duckdb::StatementReturnType GetReturnType() const override;
	duckdb::QueryResultType GetResultType() const override;
	idx_t ColumnCount() const override;
	const duckdb::vector<duckdb::string> &Names() const override;
	const duckdb::vector<duckdb::LogicalType> &Types() const override;
	duckdb::unique_ptr<duckdb::DataChunk> Fetch() override;
	iterator begin() override;
	iterator end() override;

	idx_t RowCount() const override;
	ShellColumnDataCollection Collection() override;
};

} // namespace duckdb_shell
