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
#include "duckdb/main/prepared_statement.hpp"
#include "duckdb/main/table_description.hpp"

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
	unique_ptr<duckdb::MaterializedQueryResult> Query(const string &sql);
	unique_ptr<duckdb::QueryResult> SendQuery(const string &query);
	unique_ptr<duckdb::QueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement,
	                                          duckdb::QueryParameters parameters);
	unique_ptr<duckdb::QueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement);
	vector<unique_ptr<duckdb::SQLStatement>> ExtractStatements(const string &sql);
	unique_ptr<duckdb::PreparedStatement> Prepare(const string &sql);

	//! Transaction control
	void BeginTransaction();
	void Commit();
	void Rollback();

	//! Interrupt
	void Interrupt();
	void ClearInterrupt();

	//! Table info
	unique_ptr<duckdb::TableDescription> TableInfo(const string &table_name);

	//! Context access (needed for rendering, config, Cast, etc.)
	duckdb::ClientContext &GetContext();
};

} // namespace duckdb_shell
