#include "duckdb/parser/connect_mode/connect_mode_parser.hpp"

#include "duckdb/common/string_util.hpp"

namespace duckdb {

//! Connect-mode (Layer 1) grammar — see connect_mode.gram for the canonical declaration.
//! Duplicated here as a string literal so the parser is self-contained and doesn't depend on the
//! main grammar's inlining pipeline. Keep in sync with connect_mode.gram if you edit either.
static constexpr const char *CONNECT_MODE_GRAMMAR = R"(
Program          <- Chunk (Semi Chunk)* Semi?
Chunk            <- ExecuteChunk
                  / ConnectChunk
                  / DisconnectChunk
                  / ForbiddenConnect
                  / ForbiddenDisconnect
                  / RawChunk

ConnectChunk        <- 'CONNECT' (Identifier / 'LOCAL' / StringLiteral)?
DisconnectChunk     <- 'DISCONNECT'
ExecuteChunk        <- 'CONNECT' Identifier 'EXECUTE' RawChunk
ForbiddenConnect    <- 'CONNECT' Raw
ForbiddenDisconnect <- 'DISCONNECT' Raw
RawChunk            <- !ControlKeyword Raw
ControlKeyword      <- 'CONNECT' / 'DISCONNECT'

Raw            <- (!Semi !EOI .)+
Semi           <- ';'

Identifier        <- QuotedIdentifier / PlainIdentifier
QuotedIdentifier  <- '"' [^"]* '"'
PlainIdentifier   <- [a-zA-Z_] [a-zA-Z0-9_]*
StringLiteral     <- "'" [^']* "'"
)";

ConnectModeParser::ConnectModeParser(const string &sql_p) : sql(sql_p) {
}

ConnectModeParser::~ConnectModeParser() = default;

bool ConnectModeParser::TryGetNext(ConnectModeChunk &out_chunk) {
	// TODO: tokenize input on first call, match against CONNECT_MODE_GRAMMAR, walk parse tree,
	// emit one ConnectModeChunk per top-level alternative. v0 stub: report end-of-input.
	(void)out_chunk;
	(void)CONNECT_MODE_GRAMMAR;
	return false;
}

vector<ConnectModeChunk> ConnectModeParser::AllRemaining() {
	vector<ConnectModeChunk> result;
	ConnectModeChunk chunk;
	while (TryGetNext(chunk)) {
		result.push_back(std::move(chunk));
	}
	return result;
}

} // namespace duckdb
