//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_connection.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/main/table_description.hpp"
#include "shell_prepared_statement.hpp"

namespace duckdb_shell {
using duckdb::make_uniq;
using duckdb::string;
using duckdb::unique_ptr;
using duckdb::vector;

//! ShellConnection wraps a duckdb::Connection, exposing only the methods the shell needs.
//! No virtual methods yet — polymorphism comes later when wire mode lands.
class ShellConnection {
	unique_ptr<duckdb::Connection> conn;

public:
	explicit ShellConnection(unique_ptr<duckdb::Connection> conn);
	~ShellConnection();

	//! Query execution
	unique_ptr<ShellMaterializedQueryResult> Query(const string &sql);
	unique_ptr<ShellQueryResult> SendQuery(const string &query);
	unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement,
	                                       duckdb::QueryParameters parameters);
	unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement);
	vector<unique_ptr<duckdb::SQLStatement>> ExtractStatements(const string &sql);
	unique_ptr<ShellPreparedStatement> Prepare(const string &sql);

	//! Transaction control
	void BeginTransaction();
	void Commit();
	void Rollback();

	//! Interrupt
	void Interrupt();
	void ClearInterrupt();

	//! Table info
	unique_ptr<duckdb::TableDescription> TableInfo(const string &table_name);

	//! Cast all columns in a DataChunk to VARCHAR.
	//! If complex_objects_as_json is true, nested and floating-point types are cast through JSON
	//! first to preserve structure (e.g. for JSON/JSONLINES output modes).
	unique_ptr<duckdb::DataChunk> CastToVarchar(duckdb::DataChunk &chunk, bool complex_objects_as_json = false);

	//! Context access (needed for rendering, config, Cast, etc.)
	duckdb::ClientContext &GetContext();
};

} // namespace duckdb_shell
