//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_prepared_statement_local.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_prepared_statement.hpp"
#include "duckdb/main/prepared_statement.hpp"

namespace duckdb_shell {

//! ShellPreparedStatementLocal wraps a duckdb::PreparedStatement.
class ShellPreparedStatementLocal : public ShellPreparedStatement {
public:
	explicit ShellPreparedStatementLocal(duckdb::unique_ptr<duckdb::PreparedStatement> statement);
	~ShellPreparedStatementLocal() override;

	//! Error handling
	bool HasError() const override;
	const string &GetError() const override;

	//! Execute with bind values, returning a wrapped query result
	unique_ptr<ShellQueryResult> Execute(vector<duckdb::Value> &values) override;

private:
	duckdb::unique_ptr<duckdb::PreparedStatement> statement;
};

} // namespace duckdb_shell
