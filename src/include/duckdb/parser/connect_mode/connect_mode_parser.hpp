//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/connect_mode/connect_mode_parser.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/optional_idx.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/common/vector.hpp"

namespace duckdb {

//! One chunk of input as seen by the connect-mode (Layer 1) parser.
//!
//!  - CONTROL:   a bare CONNECT or DISCONNECT statement (no EXECUTE clause). The caller hands the
//!    chunk's text to the main SQL parser for full handling.
//!  - EXECUTE:   a `CONNECT <name> EXECUTE <payload>` form. The caller dispatches `payload` to
//!    `<name>`'s catalog via RemoteExecute, without changing binding state.
//!  - FORBIDDEN: input that starts with CONNECT or DISCONNECT but doesn't match any well-formed
//!    control variant. The caller surfaces a clear parse error rather than forwarding silently.
//!  - RAW:       anything else. When CONNECTed, dispatched verbatim to the bound catalog via
//!    RemoteExecute. When not CONNECTed, fed to the main SQL parser as usual.
struct ConnectModeChunk {
	enum class Type : uint8_t { CONTROL, EXECUTE, FORBIDDEN, RAW };

	Type type;
	//! Slice of the original input that this chunk corresponds to (no surrounding `;`).
	string text;

	//! Populated only for EXECUTE chunks.
	string target;
	string payload;
};

//! Layer 1 parser: identifies CONNECT / DISCONNECT / `CONNECT name EXECUTE <raw>` statements and
//! captures everything else as opaque Raw chunks (string/comment-aware).
//!
//! Iterator-style by design: tokenize once upfront, then peel one chunk at a time. The caller
//! interleaves Next() with execution so that state changes from one chunk (binding/unbinding) are
//! visible when classifying the next.
class ConnectModeParser {
public:
	explicit ConnectModeParser(const string &sql);
	~ConnectModeParser();

	//! Return the next chunk, or nullopt if input is exhausted.
	bool TryGetNext(ConnectModeChunk &out_chunk);

	//! Convenience: collect all remaining chunks into a vector.
	vector<ConnectModeChunk> AllRemaining();

private:
	const string &sql;
	//! Position in `sql` we'll resume from on the next TryGetNext call.
	idx_t cursor = 0;
};

} // namespace duckdb
