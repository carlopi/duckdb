#include "wire_transport_mock.hpp"
#include "wire_serialization.hpp"
#include "duckdb.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/serializer/binary_serializer.hpp"
#include "duckdb/common/serializer/binary_deserializer.hpp"
#include "duckdb/common/serializer/memory_stream.hpp"
#include "duckdb/main/table_description.hpp"
#include "duckdb/main/prepared_statement.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb_shell {
using duckdb::BinaryDeserializer;
using duckdb::BinarySerializer;
using duckdb::make_uniq;
using duckdb::MemoryStream;

MockTransportLayer::MockTransportLayer(const char *path, duckdb::DBConfig &config) {
	db = make_uniq<duckdb::DuckDB>(path, &config);
}

MockTransportLayer::~MockTransportLayer() {
}

duckdb::Connection &MockTransportLayer::GetConnection(conn_id_t conn) {
	return *GetConnectionState(conn).connection;
}

MockTransportLayer::ConnectionState &MockTransportLayer::GetConnectionState(conn_id_t conn) {
	auto it = connections.find(conn);
	if (it == connections.end()) {
		throw duckdb::InternalException("MockTransportLayer: invalid connection id %llu", conn);
	}
	return it->second;
}

string MockTransportLayer::SerializeDataChunk(duckdb::DataChunk &chunk) {
	MemoryStream stream;
	BinarySerializer serializer(stream);
	serializer.Begin();
	chunk.Serialize(serializer);
	serializer.End();
	return string(duckdb::const_char_ptr_cast(stream.GetData()), stream.GetPosition());
}

string MockTransportLayer::SerializeMetadata(duckdb::QueryResult &result) {
	WireResultMetadata meta;
	meta.has_error = result.HasError();
	if (meta.has_error) {
		meta.error_message = result.GetError();
		return WireSerializer::Serialize(meta);
	}
	for (auto &name : result.names) {
		meta.column_names.push_back(name);
	}
	for (auto &type : result.types) {
		meta.column_types.push_back(LogicalTypeProperties::FromLogicalType(type));
	}
	meta.statement_return_type = static_cast<uint8_t>(result.properties.return_type);
	meta.query_result_type = static_cast<uint8_t>(result.type);
	return WireSerializer::Serialize(meta);
}

// === Connection management ===

conn_id_t MockTransportLayer::CreateConnection() {
	auto id = next_conn_id++;
	ConnectionState state;
	state.connection = make_uniq<duckdb::Connection>(*db);
	connections[id] = std::move(state);
	return id;
}

void MockTransportLayer::CloseConnection(conn_id_t conn) {
	connections.erase(conn);
}

// === Query execution ===

string MockTransportLayer::Query(conn_id_t conn, const string &sql) {
	auto &connection = GetConnection(conn);
	auto result = connection.Query(sql);
	auto blob = SerializeMetadata(*result);
	// Store the materialized result for Fetch() calls
	GetConnectionState(conn).active_result = std::move(result);
	return blob;
}

string MockTransportLayer::SendQuery(conn_id_t conn, const string &sql) {
	auto &connection = GetConnection(conn);
	auto result = connection.SendQuery(sql);
	auto blob = SerializeMetadata(*result);
	// Store the streaming result for Fetch() calls
	GetConnectionState(conn).active_result = std::move(result);
	return blob;
}

string MockTransportLayer::Prepare(conn_id_t conn, const string &sql, prep_id_t &out_prep) {
	// TODO: implement prepared statement storage
	(void)conn;
	(void)sql;
	(void)out_prep;
	WireResultMetadata meta;
	meta.has_error = true;
	meta.error_message = "Prepare not yet implemented in MockTransportLayer";
	return WireSerializer::Serialize(meta);
}

string MockTransportLayer::Execute(prep_id_t prep, const string &values_blob) {
	// TODO: implement prepared statement execution
	(void)prep;
	(void)values_blob;
	WireResultMetadata meta;
	meta.has_error = true;
	meta.error_message = "Execute not yet implemented in MockTransportLayer";
	return WireSerializer::Serialize(meta);
}

// === Data fetch ===

string MockTransportLayer::Fetch(conn_id_t conn) {
	auto &state = GetConnectionState(conn);
	if (!state.active_result) {
		return "";
	}
	auto chunk = state.active_result->Fetch();
	if (!chunk || chunk->size() == 0) {
		return "";
	}
	return SerializeDataChunk(*chunk);
}

// === CastToVarchar ===

string MockTransportLayer::CastToVarchar(conn_id_t conn, const string &chunk_blob, bool as_json) {
	// Deserialize the incoming DataChunk
	MemoryStream read_stream(duckdb::data_ptr_cast(const_cast<char *>(chunk_blob.data())),
	                         duckdb::NumericCast<duckdb::idx_t>(chunk_blob.size()));
	BinaryDeserializer deserializer(read_stream);
	auto input = make_uniq<duckdb::DataChunk>();
	deserializer.Begin();
	input->Deserialize(deserializer);
	deserializer.End();

	// Cast to varchar using connection context
	auto &connection = GetConnection(conn);
	auto result = input->CastToVarchar(*connection.context, as_json);

	// Serialize the result
	return SerializeDataChunk(*result);
}

// === Transaction control ===

void MockTransportLayer::BeginTransaction(conn_id_t conn) {
	GetConnection(conn).BeginTransaction();
}

void MockTransportLayer::Commit(conn_id_t conn) {
	GetConnection(conn).Commit();
}

void MockTransportLayer::Rollback(conn_id_t conn) {
	GetConnection(conn).Rollback();
}

bool MockTransportLayer::IsAutoCommit(conn_id_t conn) {
	return GetConnection(conn).IsAutoCommit();
}

// === Interrupt ===

void MockTransportLayer::Interrupt(conn_id_t conn) {
	GetConnection(conn).Interrupt();
}

void MockTransportLayer::ClearInterrupt(conn_id_t conn) {
	// TODO: ClearInterrupt is not on Connection, it's on ClientContext
	(void)conn;
}

// === Table info ===

string MockTransportLayer::TableInfo(conn_id_t conn, const string &table_name) {
	auto &connection = GetConnection(conn);
	auto info = connection.TableInfo(table_name);
	if (!info) {
		return "";
	}
	vector<pair<string, string>> columns;
	for (auto &col : info->columns) {
		columns.push_back({col.Name(), col.Type().ToString()});
	}
	return WireSerializer::SerializeTableInfo(columns);
}

} // namespace duckdb_shell
