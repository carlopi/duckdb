#include "shell_connection_local.hpp"
#include "shell_query_result_local.hpp"
#include "shell_prepared_statement_local.hpp"

namespace duckdb_shell {

ShellConnectionLocal::ShellConnectionLocal(unique_ptr<duckdb::Connection> conn_p) : conn(std::move(conn_p)) {
}

ShellConnectionLocal::~ShellConnectionLocal() {
}

unique_ptr<ShellMaterializedQueryResult> ShellConnectionLocal::Query(const string &sql) {
	return make_uniq<ShellMaterializedQueryResultLocal>(conn->Query(sql));
}

unique_ptr<ShellQueryResult> ShellConnectionLocal::SendQuery(const string &query) {
	return make_uniq<ShellQueryResultLocal>(conn->SendQuery(query));
}

unique_ptr<ShellQueryResult> ShellConnectionLocal::SendQuery(unique_ptr<duckdb::SQLStatement> statement,
                                                             duckdb::QueryParameters parameters) {
	return make_uniq<ShellQueryResultLocal>(conn->SendQuery(std::move(statement), parameters));
}

unique_ptr<ShellQueryResult> ShellConnectionLocal::SendQuery(unique_ptr<duckdb::SQLStatement> statement) {
	return make_uniq<ShellQueryResultLocal>(conn->SendQuery(std::move(statement)));
}

vector<unique_ptr<duckdb::SQLStatement>> ShellConnectionLocal::ExtractStatements(const string &sql) {
	return conn->ExtractStatements(sql);
}

unique_ptr<ShellPreparedStatement> ShellConnectionLocal::Prepare(const string &sql) {
	return make_uniq<ShellPreparedStatementLocal>(conn->Prepare(sql));
}

bool ShellConnectionLocal::IsAutoCommit() {
	return conn->context->transaction.IsAutoCommit();
}

void ShellConnectionLocal::BeginTransaction() {
	conn->BeginTransaction();
}

void ShellConnectionLocal::Commit() {
	conn->Commit();
}

void ShellConnectionLocal::Rollback() {
	conn->Rollback();
}

void ShellConnectionLocal::Interrupt() {
	conn->Interrupt();
}

void ShellConnectionLocal::ClearInterrupt() {
	conn->context->ClearInterrupt();
}

unique_ptr<duckdb::TableDescription> ShellConnectionLocal::TableInfo(const string &table_name) {
	return conn->TableInfo(table_name);
}

unique_ptr<duckdb::DataChunk> ShellConnectionLocal::CastToVarchar(duckdb::DataChunk &chunk,
                                                                  bool complex_objects_as_json) {
	return chunk.CastToVarchar(*conn->context, complex_objects_as_json);
}

duckdb::ClientContext &ShellConnectionLocal::GetContext() {
	return *conn->context;
}

} // namespace duckdb_shell
