//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/main/statement_iterator.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/parser/connect_mode/connect_mode_parser.hpp"

namespace duckdb {
class AttachedDatabase;
class ClientContext;
class SQLStatement;

//! Iterator that yields one SQL statement at a time from a multi-statement query, dispatching
//! Layer 1 (CONNECT-aware) chunks against the *current* ClientContext binding state. Because
//! state changes (CONNECT/DISCONNECT) executed via prior yielded statements are visible to
//! subsequent calls, the caller can interleave parse and execute correctly.
//!
//!  - CONTROL chunks (CONNECT / DISCONNECT) are parsed via the SQL parser; the resulting
//!    statements are yielded one at a time and the chokepoint won't wrap them.
//!  - EXECUTE chunks (`CONNECT <name> EXECUTE <sql>`) are pre-wrapped against the named target
//!    catalog (or run locally for `LOCAL`); the yielded statement skips the chokepoint's wrap.
//!  - RAW chunks while bound are pre-wrapped against the current binding; same skip-wrap.
//!  - RAW chunks while not bound are parsed via the SQL parser and yielded one at a time.
//!  - FORBIDDEN chunks throw a per-shape ParserException from GetNext().
class StatementIterator {
public:
	//! Kind of the next statement (peek without consuming).
	enum class StatementKind : uint8_t {
		//! No more statements.
		NONE,
		//! CONNECT or DISCONNECT — runs locally; chokepoint never wraps it.
		CONTROL,
		//! Pre-wrapped passthrough (EXECUTE or RAW-bound); chokepoint must not re-wrap.
		PASSTHROUGH,
		//! Normal local statement (RAW-unbound, or CONNECT LOCAL EXECUTE payload).
		LOCAL,
		//! Next call to GetNext() will throw with the FORBIDDEN per-shape error.
		FORBIDDEN,
	};

	DUCKDB_API StatementIterator(ClientContext &context, const string &sql);
	DUCKDB_API ~StatementIterator();

	StatementIterator(const StatementIterator &) = delete;
	StatementIterator &operator=(const StatementIterator &) = delete;

	//! Peek at what GetNext() will yield (does not consume). Returns NONE when exhausted.
	DUCKDB_API StatementKind PeekKind();

	//! Returns the next statement (null when exhausted). `out_connect_target` is the
	//! `connect_target` to pass to PendingQueryInternal / PendingStatementOrPreparedStatement
	//! (nullptr means no wrap; non-null means wrap against this catalog). Throws ParserException
	//! for FORBIDDEN chunks (per-shape message), NotImplementedException for forms not yet
	//! supported.
	DUCKDB_API unique_ptr<SQLStatement> GetNext(optional_ptr<AttachedDatabase> &out_connect_target);

private:
	void EnsureBufferFilled();

private:
	ClientContext &context;
	ConnectModeParser layer1;
	//! Buffered statements from the most recently consumed chunk (CONTROL / RAW-unbound chunks
	//! can yield multiple parsed statements; PASSTHROUGH chunks yield exactly one).
	vector<unique_ptr<SQLStatement>> buffer;
	idx_t buffer_pos = 0;
	//! Kind of the chunk that produced `buffer`. NONE when no chunk is loaded yet (or exhausted).
	StatementKind current_kind = StatementKind::NONE;
	//! For FORBIDDEN: the message to throw when GetNext is called.
	string forbidden_message;
	//! For PASSTHROUGH: the catalog the chokepoint should wrap against (already validated). The
	//! iterator owns this so a temporary returned shared_ptr stays alive until the caller has
	//! dispatched the yielded statement.
	shared_ptr<AttachedDatabase> passthrough_target;
};

} // namespace duckdb
