//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_prepared_statement_wired.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_prepared_statement.hpp"

namespace duckdb_shell {

//! Wire-mode stub for ShellPreparedStatement.
class ShellPreparedStatementWired : public ShellPreparedStatement {
public:
	ShellPreparedStatementWired();
	~ShellPreparedStatementWired() override;

	bool HasError() const override;
	const string &GetError() const override;
	unique_ptr<ShellQueryResult> Execute(vector<duckdb::Value> &values) override;
};

} // namespace duckdb_shell
