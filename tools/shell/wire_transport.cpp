#include "wire_transport.hpp"

namespace duckdb_shell {

conn_id_t TransportLayer::CreateConnection() {
	OnSend("CreateConnection", 0);
	auto result = DoCreateConnection();
	OnReceive("CreateConnection", sizeof(conn_id_t));
	return result;
}

void TransportLayer::CloseConnection(conn_id_t conn) {
	OnSend("CloseConnection", sizeof(conn_id_t));
	DoCloseConnection(conn);
	OnReceive("CloseConnection", 0);
}

string TransportLayer::Query(conn_id_t conn, const string &sql) {
	OnSend("Query", sizeof(conn_id_t) + sql.size());
	auto result = DoQuery(conn, sql);
	OnReceive("Query", result.size());
	return result;
}

string TransportLayer::SendQuery(conn_id_t conn, const string &sql) {
	OnSend("SendQuery", sizeof(conn_id_t) + sql.size());
	auto result = DoSendQuery(conn, sql);
	OnReceive("SendQuery", result.size());
	return result;
}

string TransportLayer::Prepare(conn_id_t conn, const string &sql, prep_id_t &out_prep) {
	OnSend("Prepare", sizeof(conn_id_t) + sql.size());
	auto result = DoPrepare(conn, sql, out_prep);
	OnReceive("Prepare", result.size() + sizeof(prep_id_t));
	return result;
}

string TransportLayer::Execute(prep_id_t prep, const string &values_blob) {
	OnSend("Execute", sizeof(prep_id_t) + values_blob.size());
	auto result = DoExecute(prep, values_blob);
	OnReceive("Execute", result.size());
	return result;
}

string TransportLayer::Fetch(conn_id_t conn) {
	OnSend("Fetch", sizeof(conn_id_t));
	auto result = DoFetch(conn);
	OnReceive("Fetch", result.size());
	return result;
}

string TransportLayer::CastToVarchar(conn_id_t conn, const string &chunk_blob, bool as_json) {
	OnSend("CastToVarchar", sizeof(conn_id_t) + chunk_blob.size() + 1);
	auto result = DoCastToVarchar(conn, chunk_blob, as_json);
	OnReceive("CastToVarchar", result.size());
	return result;
}

void TransportLayer::BeginTransaction(conn_id_t conn) {
	OnSend("BeginTransaction", sizeof(conn_id_t));
	DoBeginTransaction(conn);
	OnReceive("BeginTransaction", 0);
}

void TransportLayer::Commit(conn_id_t conn) {
	OnSend("Commit", sizeof(conn_id_t));
	DoCommit(conn);
	OnReceive("Commit", 0);
}

void TransportLayer::Rollback(conn_id_t conn) {
	OnSend("Rollback", sizeof(conn_id_t));
	DoRollback(conn);
	OnReceive("Rollback", 0);
}

bool TransportLayer::IsAutoCommit(conn_id_t conn) {
	OnSend("IsAutoCommit", sizeof(conn_id_t));
	auto result = DoIsAutoCommit(conn);
	OnReceive("IsAutoCommit", 1);
	return result;
}

void TransportLayer::Interrupt(conn_id_t conn) {
	OnSend("Interrupt", sizeof(conn_id_t));
	DoInterrupt(conn);
	OnReceive("Interrupt", 0);
}

void TransportLayer::ClearInterrupt(conn_id_t conn) {
	OnSend("ClearInterrupt", sizeof(conn_id_t));
	DoClearInterrupt(conn);
	OnReceive("ClearInterrupt", 0);
}

string TransportLayer::TableInfo(conn_id_t conn, const string &table_name) {
	OnSend("TableInfo", sizeof(conn_id_t) + table_name.size());
	auto result = DoTableInfo(conn, table_name);
	OnReceive("TableInfo", result.size());
	return result;
}

} // namespace duckdb_shell
