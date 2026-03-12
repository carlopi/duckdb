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
	auto col_count = chunk.ColumnCount();

	// Step 1: set up the output chunk
	auto varchar_chunk = make_uniq<duckdb::DataChunk>();
	vector<duckdb::LogicalType> all_varchar(col_count, duckdb::LogicalType::VARCHAR);
	varchar_chunk->Initialize(duckdb::Allocator::DefaultAllocator(), all_varchar);

	// Step 2: if complex_objects_as_json, pre-cast nested/floating columns through JSON
	duckdb::DataChunk json_chunk;
	if (complex_objects_as_json) {
		vector<duckdb::LogicalType> json_types;
		for (idx_t c = 0; c < col_count; c++) {
			if (RequiresJSONCast(chunk.data[c].GetType())) {
				json_types.emplace_back(duckdb::LogicalType::JSON());
			} else {
				json_types.emplace_back(chunk.data[c].GetType());
			}
		}
		json_chunk.Initialize(duckdb::Allocator::DefaultAllocator(), json_types);
		for (idx_t c = 0; c < col_count; c++) {
			duckdb::VectorOperations::Cast(context, chunk.data[c], json_chunk.data[c], chunk.size());
		}
		json_chunk.SetCardinality(chunk.size());
		json_chunk.Flatten();
	}

	// Step 3: cast all columns to VARCHAR
	auto &source = complex_objects_as_json ? json_chunk : chunk;
	for (idx_t c = 0; c < col_count; c++) {
		duckdb::VectorOperations::Cast(context, source.data[c], varchar_chunk->data[c], chunk.size());
	}
	varchar_chunk->SetCardinality(chunk.size());
	varchar_chunk->Flatten();
	return varchar_chunk;
}

duckdb::ClientContext &ShellConnection::GetContext() {
	return *conn->context;
}

} // namespace duckdb_shell
