//===----------------------------------------------------------------------===//
//                         DuckDB
//
// wire_transport_mock.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "wire_transport.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/unordered_map.hpp"

namespace duckdb {
class DuckDB;
class DBConfig;
class Connection;
class QueryResult;
class DataChunk;
} // namespace duckdb

namespace duckdb_shell {
using duckdb::unique_ptr;
using duckdb::unordered_map;

//! MockTransportLayer implements TransportLayer by owning a real DuckDB instance.
//! All data crosses the interface through serialization/deserialization — no shared memory.
class MockTransportLayer : public TransportLayer {
public:
	MockTransportLayer(const char *path, duckdb::DBConfig &config);
	~MockTransportLayer() override;

	conn_id_t CreateConnection() override;
	void CloseConnection(conn_id_t conn) override;

	WireResultMetadata Query(conn_id_t conn, const string &sql) override;
	WireResultMetadata SendQuery(conn_id_t conn, const string &sql) override;
	WireResultMetadata Prepare(conn_id_t conn, const string &sql, prep_id_t &out_prep) override;
	WireResultMetadata Execute(prep_id_t prep, const string &values_blob) override;

	string Fetch(conn_id_t conn) override;
	string CastToVarchar(conn_id_t conn, const string &chunk_blob, bool as_json) override;

	void BeginTransaction(conn_id_t conn) override;
	void Commit(conn_id_t conn) override;
	void Rollback(conn_id_t conn) override;
	bool IsAutoCommit(conn_id_t conn) override;

	void Interrupt(conn_id_t conn) override;
	void ClearInterrupt(conn_id_t conn) override;

	vector<pair<string, string>> TableInfo(conn_id_t conn, const string &table_name) override;

private:
	struct ConnectionState {
		unique_ptr<duckdb::Connection> connection;
		//! The active streaming result (if any) for Fetch() calls
		unique_ptr<duckdb::QueryResult> active_result;
	};

	duckdb::Connection &GetConnection(conn_id_t conn);
	ConnectionState &GetConnectionState(conn_id_t conn);
	WireResultMetadata BuildMetadata(duckdb::QueryResult &result);
	string SerializeDataChunk(duckdb::DataChunk &chunk);

	unique_ptr<duckdb::DuckDB> db;
	unordered_map<conn_id_t, ConnectionState> connections;
	conn_id_t next_conn_id = 1;
	prep_id_t next_prep_id = 1;
};

} // namespace duckdb_shell
