//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_connection_wired.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_connection.hpp"

namespace duckdb_shell {

//! Wire-mode stub for ShellConnection.
class ShellConnectionWired : public ShellConnection {
public:
	ShellConnectionWired();
	~ShellConnectionWired() override;

	unique_ptr<ShellMaterializedQueryResult> Query(const string &sql) override;
	unique_ptr<ShellQueryResult> SendQuery(const string &query) override;
	unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement,
	                                       duckdb::QueryParameters parameters) override;
	unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement) override;
	vector<unique_ptr<duckdb::SQLStatement>> ExtractStatements(const string &sql) override;
	unique_ptr<ShellPreparedStatement> Prepare(const string &sql) override;

	void BeginTransaction() override;
	void Commit() override;
	void Rollback() override;

	void Interrupt() override;
	void ClearInterrupt() override;

	unique_ptr<duckdb::TableDescription> TableInfo(const string &table_name) override;
	unique_ptr<duckdb::DataChunk> CastToVarchar(duckdb::DataChunk &chunk, bool complex_objects_as_json = false) override;
};

} // namespace duckdb_shell
