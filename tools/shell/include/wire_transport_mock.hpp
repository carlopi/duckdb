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

protected:
	void OnSend(const char *method, idx_t bytes) override;
	void OnReceive(const char *method, idx_t bytes) override;

	conn_id_t DoCreateConnection() override;
	void DoCloseConnection(conn_id_t conn) override;

	string DoQuery(conn_id_t conn, const string &sql) override;
	string DoSendQuery(conn_id_t conn, const string &sql) override;
	string DoPrepare(conn_id_t conn, const string &sql, prep_id_t &out_prep) override;
	string DoExecute(prep_id_t prep, const string &values_blob) override;

	string DoFetch(conn_id_t conn) override;
	string DoCastToVarchar(conn_id_t conn, const string &chunk_blob, bool as_json) override;

	void DoBeginTransaction(conn_id_t conn) override;
	void DoCommit(conn_id_t conn) override;
	void DoRollback(conn_id_t conn) override;
	bool DoIsAutoCommit(conn_id_t conn) override;

	void DoInterrupt(conn_id_t conn) override;
	void DoClearInterrupt(conn_id_t conn) override;

	string DoTableInfo(conn_id_t conn, const string &table_name) override;

private:
	struct ConnectionState {
		unique_ptr<duckdb::Connection> connection;
		//! The active streaming result (if any) for Fetch() calls
		unique_ptr<duckdb::QueryResult> active_result;
	};

	duckdb::Connection &GetConnection(conn_id_t conn);
	ConnectionState &GetConnectionState(conn_id_t conn);
	string SerializeMetadata(duckdb::QueryResult &result);
	string SerializeDataChunk(duckdb::DataChunk &chunk);

	unique_ptr<duckdb::DuckDB> db;
	unordered_map<conn_id_t, ConnectionState> connections;
	conn_id_t next_conn_id = 1;
	prep_id_t next_prep_id = 1;
};

} // namespace duckdb_shell
