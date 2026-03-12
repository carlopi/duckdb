//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_connection_local.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_connection.hpp"
#include "duckdb.hpp"

namespace duckdb_shell {
using duckdb::make_uniq;

//! ShellConnectionLocal wraps a duckdb::Connection.
class ShellConnectionLocal : public ShellConnection {
public:
	explicit ShellConnectionLocal(unique_ptr<duckdb::Connection> conn);
	~ShellConnectionLocal() override;

	//! Query execution
	unique_ptr<ShellMaterializedQueryResult> Query(const string &sql) override;
	unique_ptr<ShellQueryResult> SendQuery(const string &query) override;
	unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement,
	                                       duckdb::QueryParameters parameters) override;
	unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement) override;
	vector<unique_ptr<duckdb::SQLStatement>> ExtractStatements(const string &sql) override;
	unique_ptr<ShellPreparedStatement> Prepare(const string &sql) override;

	//! Transaction control
	bool IsAutoCommit() override;
	void BeginTransaction() override;
	void Commit() override;
	void Rollback() override;

	//! Interrupt
	void Interrupt() override;
	void ClearInterrupt() override;

	//! Table info
	unique_ptr<duckdb::TableDescription> TableInfo(const string &table_name) override;

	//! Cast all columns in a DataChunk to VARCHAR.
	unique_ptr<duckdb::DataChunk> CastToVarchar(duckdb::DataChunk &chunk, bool complex_objects_as_json = false) override;

	//! Engine-only: context access (needed for rendering, config, Cast, etc.)
	duckdb::ClientContext &GetContext();

private:
	unique_ptr<duckdb::Connection> conn;
};

} // namespace duckdb_shell
