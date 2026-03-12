#include "shell_connection.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"

namespace duckdb_shell {

ShellConnection::ShellConnection(unique_ptr<duckdb::Connection> conn_p) : conn(std::move(conn_p)) {
}

ShellConnection::~ShellConnection() {
}

unique_ptr<duckdb::MaterializedQueryResult> ShellConnection::Query(const string &sql) {
	return conn->Query(sql);
}

unique_ptr<duckdb::QueryResult> ShellConnection::SendQuery(const string &query) {
	return conn->SendQuery(query);
}

unique_ptr<duckdb::QueryResult> ShellConnection::SendQuery(unique_ptr<duckdb::SQLStatement> statement,
                                                           duckdb::QueryParameters parameters) {
	return conn->SendQuery(std::move(statement), parameters);
}

unique_ptr<duckdb::QueryResult> ShellConnection::SendQuery(unique_ptr<duckdb::SQLStatement> statement) {
	return conn->SendQuery(std::move(statement));
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

static bool RequiresJSONCast(const duckdb::LogicalType &type) {
	if (type.IsNested()) {
		return true;
	}
	if (type.IsFloating()) {
		return true;
	}
	return false;
}

unique_ptr<duckdb::DataChunk> ShellConnection::CastToVarchar(duckdb::DataChunk &chunk, bool complex_objects_as_json) {
	auto &context = *conn->context;

	// If complex_objects_as_json, first cast nested/floating types through JSON
	if (complex_objects_as_json) {
		duckdb::DataChunk json_chunk;
		vector<duckdb::LogicalType> target_types;
		for (idx_t c = 0; c < chunk.ColumnCount(); c++) {
			if (RequiresJSONCast(chunk.data[c].GetType())) {
				target_types.emplace_back(duckdb::LogicalType::JSON());
			} else {
				target_types.emplace_back(duckdb::LogicalType::VARCHAR);
			}
		}
		json_chunk.Initialize(duckdb::Allocator::DefaultAllocator(), target_types);
		for (idx_t c = 0; c < chunk.ColumnCount(); c++) {
			duckdb::VectorOperations::Cast(context, chunk.data[c], json_chunk.data[c], chunk.size());
		}
		json_chunk.SetCardinality(chunk.size());
		json_chunk.Flatten();
		// Now cast the JSON chunk to all-VARCHAR
		return CastToVarchar(json_chunk, false);
	}

	// Cast all columns to VARCHAR
	auto varchar_chunk = make_uniq<duckdb::DataChunk>();
	vector<duckdb::LogicalType> all_varchar;
	for (idx_t c = 0; c < chunk.ColumnCount(); c++) {
		all_varchar.emplace_back(duckdb::LogicalType::VARCHAR);
	}
	varchar_chunk->Initialize(duckdb::Allocator::DefaultAllocator(), all_varchar);
	for (idx_t c = 0; c < chunk.ColumnCount(); c++) {
		duckdb::VectorOperations::Cast(context, chunk.data[c], varchar_chunk->data[c], chunk.size());
	}
	varchar_chunk->SetCardinality(chunk.size());
	varchar_chunk->Flatten();
	return varchar_chunk;
}

duckdb::ClientContext &ShellConnection::GetContext() {
	return *conn->context;
}

} // namespace duckdb_shell
