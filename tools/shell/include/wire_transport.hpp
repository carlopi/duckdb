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
//!
//! Public methods are concrete — they call OnSend/OnReceive hooks around the
//! virtual DoXxx implementation. Override OnSend/OnReceive for logging/tracing.
class TransportLayer {
public:
	virtual ~TransportLayer() = default;

	// === Public API (non-virtual, calls hooks + DoXxx) ===

	conn_id_t CreateConnection();
	void CloseConnection(conn_id_t conn);

	// TODO: Do we need both Query and SendQuery on the wire? Both return metadata + chunked
	// Fetch. The materialized vs streaming distinction is a server-side detail — the client
	// fetches chunks either way. Could unify into a single method with a flag.
	string Query(conn_id_t conn, const string &sql);
	string SendQuery(conn_id_t conn, const string &sql);
	string Prepare(conn_id_t conn, const string &sql, prep_id_t &out_prep);
	string Execute(prep_id_t prep, const string &values_blob);

	string Fetch(conn_id_t conn);
	// TODO: CastToVarchar sends a full DataChunk round-trip (serialize, send, cast server-side,
	// serialize result, send back). Could the server cast to VARCHAR before sending chunks
	// in Fetch(), avoiding the extra round-trip? Needs to know as_json flag upfront.
	string CastToVarchar(conn_id_t conn, const string &chunk_blob, bool as_json);

	void BeginTransaction(conn_id_t conn);
	void Commit(conn_id_t conn);
	void Rollback(conn_id_t conn);
	bool IsAutoCommit(conn_id_t conn);

	void Interrupt(conn_id_t conn);
	void ClearInterrupt(conn_id_t conn);

	// TODO: TableInfo reconstructs LogicalType from type name strings on the client side
	// (TransformStringToLogicalTypeId) — this loses extension types. Do we need TableInfo
	// on the wire at all, or could the shell use a query instead?
	string TableInfo(conn_id_t conn, const string &table_name);

protected:
	// === Hooks — override for logging/tracing. Default: NOP. ===
	virtual void OnSend(const char *method, idx_t bytes) {
		(void)method;
		(void)bytes;
	}
	virtual void OnReceive(const char *method, idx_t bytes) {
		(void)method;
		(void)bytes;
	}

	// === Virtual implementation — subclasses implement these ===
	virtual conn_id_t DoCreateConnection() = 0;
	virtual void DoCloseConnection(conn_id_t conn) = 0;

	virtual string DoQuery(conn_id_t conn, const string &sql) = 0;
	virtual string DoSendQuery(conn_id_t conn, const string &sql) = 0;
	virtual string DoPrepare(conn_id_t conn, const string &sql, prep_id_t &out_prep) = 0;
	virtual string DoExecute(prep_id_t prep, const string &values_blob) = 0;

	virtual string DoFetch(conn_id_t conn) = 0;
	virtual string DoCastToVarchar(conn_id_t conn, const string &chunk_blob, bool as_json) = 0;

	virtual void DoBeginTransaction(conn_id_t conn) = 0;
	virtual void DoCommit(conn_id_t conn) = 0;
	virtual void DoRollback(conn_id_t conn) = 0;
	virtual bool DoIsAutoCommit(conn_id_t conn) = 0;

	virtual void DoInterrupt(conn_id_t conn) = 0;
	virtual void DoClearInterrupt(conn_id_t conn) = 0;

	virtual string DoTableInfo(conn_id_t conn, const string &table_name) = 0;
};

} // namespace duckdb_shell
