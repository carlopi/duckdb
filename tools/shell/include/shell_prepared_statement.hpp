//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_prepared_statement.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/types.hpp"

namespace duckdb_shell {
using duckdb::string;
using duckdb::unique_ptr;
using duckdb::vector;
class ShellQueryResult;

//! ShellPreparedStatement is an abstract interface for a prepared statement.
class ShellPreparedStatement {
public:
	virtual ~ShellPreparedStatement() = default;

	//! Error handling
	virtual bool HasError() const = 0;
	virtual const string &GetError() const = 0;

	//! Execute with bind values, returning a wrapped query result
	virtual unique_ptr<ShellQueryResult> Execute(vector<duckdb::Value> &values) = 0;
};

} // namespace duckdb_shell
