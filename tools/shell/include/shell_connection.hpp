//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_connection.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/parser/sql_statement.hpp"
#include "duckdb/main/query_result.hpp"
#include "duckdb/main/table_description.hpp"
#include "duckdb/common/box_renderer_context.hpp"

namespace duckdb_shell {
using duckdb::string;
using duckdb::unique_ptr;
using duckdb::vector;
class ShellQueryResult;
class ShellMaterializedQueryResult;
class ShellPreparedStatement;

//! ShellConnection is an abstract interface for a database connection.
class ShellConnection {
public:
	virtual ~ShellConnection() = default;

	//! Query execution
	virtual unique_ptr<ShellMaterializedQueryResult> Query(const string &sql) = 0;
	virtual unique_ptr<ShellQueryResult> SendQuery(const string &query) = 0;
	virtual unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement,
	                                               duckdb::QueryParameters parameters) = 0;
	virtual unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement) = 0;
	virtual vector<unique_ptr<duckdb::SQLStatement>> ExtractStatements(const string &sql) = 0;
	virtual unique_ptr<ShellPreparedStatement> Prepare(const string &sql) = 0;

	//! Transaction control
	virtual bool IsAutoCommit() = 0;
	virtual void BeginTransaction() = 0;
	virtual void Commit() = 0;
	virtual void Rollback() = 0;

	//! Interrupt
	virtual void Interrupt() = 0;
	virtual void ClearInterrupt() = 0;

	//! Table info
	virtual unique_ptr<duckdb::TableDescription> TableInfo(const string &table_name) = 0;

	//! Cast all columns in a DataChunk to VARCHAR.
	virtual unique_ptr<duckdb::DataChunk> CastToVarchar(duckdb::DataChunk &chunk,
	                                                    bool complex_objects_as_json = false) = 0;

	//! Create a BoxRendererContext for use by the BoxRenderer.
	virtual unique_ptr<duckdb::BoxRendererContext> CreateBoxRendererContext() = 0;
};

} // namespace duckdb_shell
