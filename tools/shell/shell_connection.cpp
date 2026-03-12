#include "shell_connection.hpp"

namespace duckdb_shell {

ShellConnection::ShellConnection(unique_ptr<duckdb::Connection> conn_p) : conn(std::move(conn_p)) {
}

ShellConnection::~ShellConnection() {
}

unique_ptr<ShellMaterializedQueryResult> ShellConnection::Query(const string &sql) {
	return make_uniq<ShellMaterializedQueryResult>(conn->Query(sql));
}

unique_ptr<ShellQueryResult> ShellConnection::SendQuery(const string &query) {
	return make_uniq<ShellQueryResult>(conn->SendQuery(query));
}

unique_ptr<ShellQueryResult> ShellConnection::SendQuery(unique_ptr<duckdb::SQLStatement> statement,
                                                        duckdb::QueryParameters parameters) {
	return make_uniq<ShellQueryResult>(conn->SendQuery(std::move(statement), parameters));
}

unique_ptr<ShellQueryResult> ShellConnection::SendQuery(unique_ptr<duckdb::SQLStatement> statement) {
	return make_uniq<ShellQueryResult>(conn->SendQuery(std::move(statement)));
}

vector<unique_ptr<duckdb::SQLStatement>> ShellConnection::ExtractStatements(const string &sql) {
	return conn->ExtractStatements(sql);
}

unique_ptr<duckdb::PreparedStatement> ShellConnection::Prepare(const string &sql) {
	return conn->Prepare(sql);
}

void ShellConnection::BeginTransaction() {
	conn->BeginTransaction();
}

void ShellConnection::Commit() {
	conn->Commit();
}

void ShellConnection::Rollback() {
	conn->Rollback();
}

void ShellConnection::Interrupt() {
	conn->Interrupt();
}

void ShellConnection::ClearInterrupt() {
	conn->context->ClearInterrupt();
}

unique_ptr<duckdb::TableDescription> ShellConnection::TableInfo(const string &table_name) {
	return conn->TableInfo(table_name);
}

unique_ptr<duckdb::DataChunk> ShellConnection::CastToVarchar(duckdb::DataChunk &chunk, bool complex_objects_as_json) {
	return chunk.CastToVarchar(*conn->context, complex_objects_as_json);
}

duckdb::ClientContext &ShellConnection::GetContext() {
	return *conn->context;
}

} // namespace duckdb_shell
