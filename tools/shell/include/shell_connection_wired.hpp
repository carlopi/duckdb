//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_connection_wired.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_connection.hpp"
#include "wire_transport.hpp"

namespace duckdb_shell {

//! Wire-mode implementation of ShellConnection.
//! Holds a connection ID and a pointer to the TransportLayer — no engine types.
class ShellConnectionWired : public ShellConnection {
public:
	ShellConnectionWired(conn_id_t conn_id, TransportLayer &transport);
	~ShellConnectionWired() override;

	unique_ptr<ShellMaterializedQueryResult> Query(const string &sql) override;
	unique_ptr<ShellQueryResult> SendQuery(const string &query) override;
	unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement,
	                                       duckdb::QueryParameters parameters) override;
	unique_ptr<ShellQueryResult> SendQuery(unique_ptr<duckdb::SQLStatement> statement) override;
	vector<unique_ptr<duckdb::SQLStatement>> ExtractStatements(const string &sql) override;
	unique_ptr<ShellPreparedStatement> Prepare(const string &sql) override;

	bool IsAutoCommit() override;
	void BeginTransaction() override;
	void Commit() override;
	void Rollback() override;

	void Interrupt() override;
	void ClearInterrupt() override;

	unique_ptr<duckdb::TableDescription> TableInfo(const string &table_name) override;
	unique_ptr<duckdb::DataChunk> CastToVarchar(duckdb::DataChunk &chunk,
	                                            bool complex_objects_as_json = false) override;
	unique_ptr<duckdb::BoxRendererContext> CreateBoxRendererContext() override;

private:
	conn_id_t conn_id;
	TransportLayer &transport;
};

} // namespace duckdb_shell
