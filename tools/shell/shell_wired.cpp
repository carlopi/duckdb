#include "shell_prepared_statement_wired.hpp"
#include "shell_query_result_wired.hpp"
#include "shell_connection_wired.hpp"
#include "shell_duckdb_wired.hpp"
#include "shell_db_config.hpp"
#include "wire_transport.hpp"
#ifdef DUCKDB_SHELL_WIRE_TEST
#include "wire_transport_mock.hpp"
#include "duckdb/main/config.hpp"
#endif
#include "duckdb/common/serializer/binary_serializer.hpp"
#include "duckdb/common/serializer/binary_deserializer.hpp"
#include "duckdb/common/serializer/memory_stream.hpp"
#include "duckdb/common/box_renderer_context.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/parser/parser.hpp"

namespace duckdb_shell {
using duckdb::BinaryDeserializer;
using duckdb::BinarySerializer;
using duckdb::make_uniq;
using duckdb::MemoryStream;

// ===== Helper: deserialize a DataChunk from binary blob =====

static duckdb::unique_ptr<duckdb::DataChunk> DeserializeDataChunk(const string &blob) {
	if (blob.empty()) {
		return nullptr;
	}
	MemoryStream stream(duckdb::data_ptr_cast(const_cast<char *>(blob.data())),
	                    duckdb::NumericCast<duckdb::idx_t>(blob.size()));
	BinaryDeserializer deserializer(stream);
	auto chunk = make_uniq<duckdb::DataChunk>();
	deserializer.Begin();
	chunk->Deserialize(deserializer);
	deserializer.End();
	return chunk;
}

static string SerializeDataChunk(duckdb::DataChunk &chunk) {
	MemoryStream stream;
	BinarySerializer serializer(stream);
	serializer.Begin();
	chunk.Serialize(serializer);
	serializer.End();
	return string(duckdb::const_char_ptr_cast(stream.GetData()), stream.GetPosition());
}

// ===== ShellPreparedStatementWired =====

ShellPreparedStatementWired::ShellPreparedStatementWired() {
}

ShellPreparedStatementWired::~ShellPreparedStatementWired() {
}

bool ShellPreparedStatementWired::HasError() const {
	// TODO: wire mode — prepared statements
	return true;
}

const string &ShellPreparedStatementWired::GetError() const {
	static const string err = "prepared statements not yet implemented in wire mode";
	return err;
}

unique_ptr<ShellQueryResult> ShellPreparedStatementWired::Execute(vector<duckdb::Value> &values) {
	// TODO: wire mode — prepared statements
	WireResultMetadata meta;
	meta.has_error = true;
	meta.error_message = "not implemented";
	return make_uniq<ShellQueryResultWired>(std::move(meta), nullptr);
}

// ===== ShellQueryResultWired =====

ShellQueryResultWired::ShellQueryResultWired(WireResultMetadata metadata, TransportLayer *transport)
    : metadata(std::move(metadata)), transport(transport), types(this->metadata.column_types) {
}

ShellQueryResultWired::~ShellQueryResultWired() {
}

bool ShellQueryResultWired::HasError() const {
	return metadata.has_error;
}

const duckdb::string &ShellQueryResultWired::GetError() const {
	return metadata.error_message;
}

duckdb::StatementReturnType ShellQueryResultWired::GetReturnType() const {
	return static_cast<duckdb::StatementReturnType>(metadata.statement_return_type);
}

duckdb::QueryResultType ShellQueryResultWired::GetResultType() const {
	return static_cast<duckdb::QueryResultType>(metadata.query_result_type);
}

idx_t ShellQueryResultWired::ColumnCount() const {
	return metadata.column_names.size();
}

const duckdb::vector<duckdb::string> &ShellQueryResultWired::Names() const {
	return metadata.column_names;
}

const duckdb::vector<LogicalTypeProperties> &ShellQueryResultWired::Types() const {
	return types;
}

duckdb::unique_ptr<duckdb::DataChunk> ShellQueryResultWired::Fetch() {
	if (!transport || !conn_id.IsValid()) {
		return nullptr;
	}
	auto blob = transport->Fetch(conn_id.GetIndex());
	return DeserializeDataChunk(blob);
}

ShellQueryResult::iterator ShellQueryResultWired::begin() {
	// TODO: wire mode — streaming iteration
	return ShellQueryResult::iterator(nullptr);
}

ShellQueryResult::iterator ShellQueryResultWired::end() {
	return ShellQueryResult::iterator(nullptr);
}

// ===== ShellMaterializedQueryResultWired =====

ShellMaterializedQueryResultWired::ShellMaterializedQueryResultWired(WireResultMetadata metadata,
                                                                     TransportLayer *transport)
    : metadata(std::move(metadata)), transport(transport), types(this->metadata.column_types) {
}

ShellMaterializedQueryResultWired::~ShellMaterializedQueryResultWired() {
}

bool ShellMaterializedQueryResultWired::HasError() const {
	return metadata.has_error;
}

const duckdb::string &ShellMaterializedQueryResultWired::GetError() const {
	return metadata.error_message;
}

duckdb::StatementReturnType ShellMaterializedQueryResultWired::GetReturnType() const {
	return static_cast<duckdb::StatementReturnType>(metadata.statement_return_type);
}

duckdb::QueryResultType ShellMaterializedQueryResultWired::GetResultType() const {
	return static_cast<duckdb::QueryResultType>(metadata.query_result_type);
}

idx_t ShellMaterializedQueryResultWired::ColumnCount() const {
	return metadata.column_names.size();
}

const duckdb::vector<duckdb::string> &ShellMaterializedQueryResultWired::Names() const {
	return metadata.column_names;
}

const duckdb::vector<LogicalTypeProperties> &ShellMaterializedQueryResultWired::Types() const {
	return types;
}

duckdb::unique_ptr<duckdb::DataChunk> ShellMaterializedQueryResultWired::Fetch() {
	if (!transport || !conn_id.IsValid()) {
		return nullptr;
	}
	auto blob = transport->Fetch(conn_id.GetIndex());
	return DeserializeDataChunk(blob);
}

ShellQueryResult::iterator ShellMaterializedQueryResultWired::begin() {
	// TODO: wire mode — streaming iteration
	return ShellQueryResult::iterator(nullptr);
}

ShellQueryResult::iterator ShellMaterializedQueryResultWired::end() {
	return ShellQueryResult::iterator(nullptr);
}

void ShellMaterializedQueryResultWired::Materialize() {
	if (collection) {
		return;
	}
	auto &allocator = duckdb::Allocator::DefaultAllocator();
	// Fetch all chunks first, then create the collection using the types from the first chunk
	vector<unique_ptr<duckdb::DataChunk>> chunks;
	while (true) {
		auto chunk = Fetch();
		if (!chunk || chunk->size() == 0) {
			break;
		}
		chunks.push_back(std::move(chunk));
	}
	if (chunks.empty()) {
		// Create an empty collection with VARCHAR types as fallback
		duckdb::vector<duckdb::LogicalType> varchar_types(ColumnCount(), duckdb::LogicalType::VARCHAR);
		collection = make_uniq<duckdb::ColumnDataCollection>(allocator, varchar_types);
		return;
	}
	// Use the types from the first deserialized chunk
	collection = make_uniq<duckdb::ColumnDataCollection>(allocator, chunks[0]->GetTypes());
	for (auto &chunk : chunks) {
		collection->Append(*chunk);
	}
}

idx_t ShellMaterializedQueryResultWired::RowCount() const {
	if (collection) {
		return collection->Count();
	}
	return 0;
}

ShellColumnDataCollection ShellMaterializedQueryResultWired::Collection() {
	Materialize();
	return ShellColumnDataCollection(*collection);
}

// ===== ShellConnectionWired =====

ShellConnectionWired::ShellConnectionWired(conn_id_t conn_id, TransportLayer &transport)
    : conn_id(conn_id), transport(transport) {
}

ShellConnectionWired::~ShellConnectionWired() {
	transport.CloseConnection(conn_id);
}

unique_ptr<ShellMaterializedQueryResult> ShellConnectionWired::Query(const string &sql) {
	auto meta = transport.Query(conn_id, sql);
	auto result = make_uniq<ShellMaterializedQueryResultWired>(std::move(meta), &transport);
	result->conn_id = conn_id;
	return result;
}

unique_ptr<ShellQueryResult> ShellConnectionWired::SendQuery(const string &query) {
	auto meta = transport.SendQuery(conn_id, query);
	auto result = make_uniq<ShellQueryResultWired>(std::move(meta), &transport);
	result->conn_id = conn_id;
	return result;
}

unique_ptr<ShellQueryResult> ShellConnectionWired::SendQuery(unique_ptr<duckdb::SQLStatement> statement,
                                                             duckdb::QueryParameters parameters) {
	if (parameters.output_type == duckdb::QueryResultOutputType::FORCE_MATERIALIZED) {
		return Query(statement->query);
	}
	return SendQuery(statement->query);
}

unique_ptr<ShellQueryResult> ShellConnectionWired::SendQuery(unique_ptr<duckdb::SQLStatement> statement) {
	return SendQuery(statement->query);
}

vector<unique_ptr<duckdb::SQLStatement>> ShellConnectionWired::ExtractStatements(const string &sql) {
	// Parse client-side — parser is available in client_types
	duckdb::Parser parser;
	parser.ParseQuery(sql);
	return std::move(parser.statements);
}

unique_ptr<ShellPreparedStatement> ShellConnectionWired::Prepare(const string &sql) {
	// TODO: wire mode — prepared statements
	return make_uniq<ShellPreparedStatementWired>();
}

bool ShellConnectionWired::IsAutoCommit() {
	return transport.IsAutoCommit(conn_id);
}

void ShellConnectionWired::BeginTransaction() {
	transport.BeginTransaction(conn_id);
}

void ShellConnectionWired::Commit() {
	transport.Commit(conn_id);
}

void ShellConnectionWired::Rollback() {
	transport.Rollback(conn_id);
}

void ShellConnectionWired::Interrupt() {
	transport.Interrupt(conn_id);
}

void ShellConnectionWired::ClearInterrupt() {
	transport.ClearInterrupt(conn_id);
}

unique_ptr<duckdb::TableDescription> ShellConnectionWired::TableInfo(const string &table_name) {
	auto columns = transport.TableInfo(conn_id, table_name);
	if (columns.empty()) {
		return nullptr;
	}
	auto info = make_uniq<duckdb::TableDescription>("", "", table_name);
	for (auto &col : columns) {
		info->columns.emplace_back(col.first, duckdb::LogicalType(duckdb::TransformStringToLogicalTypeId(col.second)));
	}
	return info;
}

unique_ptr<duckdb::DataChunk> ShellConnectionWired::CastToVarchar(duckdb::DataChunk &chunk,
                                                                  bool complex_objects_as_json) {
	auto blob = SerializeDataChunk(chunk);
	auto result_blob = transport.CastToVarchar(conn_id, blob, complex_objects_as_json);
	return DeserializeDataChunk(result_blob);
}

// ===== WiredBoxRendererContext =====

class WiredBoxRendererContext : public duckdb::BoxRendererContext {
public:
	bool IsInterrupted() const override {
		return false;
	}
	duckdb::Allocator &GetAllocator() override {
		return duckdb::Allocator::DefaultAllocator();
	}

protected:
	void CastToVarchar(duckdb::DataChunk &source, duckdb::DataChunk &result, duckdb::idx_t count,
	                   bool as_json) override {
		for (duckdb::idx_t c = 0; c < source.ColumnCount(); c++) {
			duckdb::VectorOperations::DefaultCast(source.data[c], result.data[c], count);
		}
	}
};

unique_ptr<duckdb::BoxRendererContext> ShellConnectionWired::CreateBoxRendererContext() {
	return make_uniq<WiredBoxRendererContext>();
}

// ===== ShellDuckDBWired =====

ShellDuckDBWired::ShellDuckDBWired(const char *path, ShellDBConfig &config) {
#ifdef DUCKDB_SHELL_WIRE_TEST
	// Wire test mode: use MockTransportLayer backed by real DuckDB
	duckdb::DBConfig db_config;
	db_config.options.access_mode = config.GetAccessMode();
	for (auto &option : config.GetOptions()) {
		db_config.SetOptionByName(option.first, option.second);
	}
	auto &compat = config.GetSerializationCompatibility();
	if (!compat.empty()) {
		db_config.SetSerializationCompatibility(compat);
	}
	for (auto &error : config.GetCustomErrors()) {
		db_config.AddCustomError(error.type, error.message);
	}
	transport = make_uniq<MockTransportLayer>(path, db_config);
#else
	// TODO: real wire transport — connect to remote server
	(void)path;
	(void)config;
#endif
	is_open = true;
}

ShellDuckDBWired::~ShellDuckDBWired() {
}

bool ShellDuckDBWired::IsOpen() const {
	return is_open;
}

void ShellDuckDBWired::Reset() {
	transport.reset();
	is_open = false;
}

TransportLayer &ShellDuckDBWired::GetTransport() {
	return *transport;
}

unique_ptr<ShellConnection> ShellDuckDBWired::CreateConnection() {
	auto conn_id = transport->CreateConnection();
	return make_uniq<ShellConnectionWired>(conn_id, *transport);
}

#if defined(DUCKDB_SHELL_WIRE_MODE) || defined(DUCKDB_SHELL_WIRE_TEST)
unique_ptr<ShellDuckDB> ShellDuckDB::Create(const char *path, ShellDBConfig &config) {
	return make_uniq<ShellDuckDBWired>(path, config);
}
#endif

} // namespace duckdb_shell
