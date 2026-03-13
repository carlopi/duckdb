//===----------------------------------------------------------------------===//
//                         DuckDB
//
// wire_transport.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/types.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/pair.hpp"
#include "logical_type_properties.hpp"

namespace duckdb_shell {
using duckdb::idx_t;
using duckdb::pair;
using duckdb::string;
using duckdb::vector;

using conn_id_t = uint64_t;
using prep_id_t = uint64_t;

//! Metadata returned from query execution — only simple types.
struct WireResultMetadata {
	bool has_error = false;
	string error_message;
	vector<string> column_names;
	//! Per-column type info — name + rendering flags
	vector<LogicalTypeProperties> column_types;
	//! StatementReturnType as uint8_t
	uint8_t statement_return_type = 0;
	//! QueryResultType as uint8_t
	uint8_t query_result_type = 0;
};

//! TransportLayer is the abstract protocol boundary between the shell client and the server.
//! All data crossing this interface is binary: scalars or serialized blobs.
//! No engine types (DataChunk, LogicalType, etc.) appear in the interface.
class TransportLayer {
public:
	virtual ~TransportLayer() = default;

	// === Connection management ===
	virtual conn_id_t CreateConnection() = 0;
	virtual void CloseConnection(conn_id_t conn) = 0;

	// === Query execution ===
	//! Execute a query and materialize the result. Returns serialized WireResultMetadata.
	virtual string Query(conn_id_t conn, const string &sql) = 0;
	//! Execute a query in streaming mode. Returns serialized WireResultMetadata.
	virtual string SendQuery(conn_id_t conn, const string &sql) = 0;
	//! Prepare a statement. Returns serialized WireResultMetadata, sets out_prep.
	virtual string Prepare(conn_id_t conn, const string &sql, prep_id_t &out_prep) = 0;
	//! Execute a prepared statement with serialized parameter values. Returns serialized WireResultMetadata.
	virtual string Execute(prep_id_t prep, const string &values_blob) = 0;

	// === Data fetch ===
	//! Fetch next chunk as serialized binary. Empty string = no more data.
	virtual string Fetch(conn_id_t conn) = 0;

	// === CastToVarchar ===
	//! Cast a serialized DataChunk to VARCHAR columns. Returns serialized DataChunk.
	virtual string CastToVarchar(conn_id_t conn, const string &chunk_blob, bool as_json) = 0;

	// === Transaction control ===
	virtual void BeginTransaction(conn_id_t conn) = 0;
	virtual void Commit(conn_id_t conn) = 0;
	virtual void Rollback(conn_id_t conn) = 0;
	virtual bool IsAutoCommit(conn_id_t conn) = 0;

	// === Interrupt ===
	virtual void Interrupt(conn_id_t conn) = 0;
	virtual void ClearInterrupt(conn_id_t conn) = 0;

	// === Table info ===
	//! Returns serialized column (name, type_string) pairs. Empty string = table not found.
	virtual string TableInfo(conn_id_t conn, const string &table_name) = 0;
};

} // namespace duckdb_shell
