//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_prepared_statement.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/main/prepared_statement.hpp"
#include "shell_query_result.hpp"

namespace duckdb_shell {
using duckdb::unique_ptr;

//! ShellPreparedStatement wraps a duckdb::PreparedStatement.
class ShellPreparedStatement {
public:
	explicit ShellPreparedStatement(duckdb::unique_ptr<duckdb::PreparedStatement> statement);
	~ShellPreparedStatement();

	//! Error handling
	bool HasError() const;
	const duckdb::string &GetError() const;

	//! Execute with bind values, returning a wrapped query result
	unique_ptr<ShellQueryResult> Execute(duckdb::vector<duckdb::Value> &values);

private:
	duckdb::unique_ptr<duckdb::PreparedStatement> statement;
};

} // namespace duckdb_shell
