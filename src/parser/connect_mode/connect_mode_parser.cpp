#include "duckdb/parser/connect_mode/connect_mode_parser.hpp"

#include "duckdb/parser/peg/matcher.hpp"
#include "duckdb/parser/peg/tokenizer/parser_tokenizer.hpp"
#include "duckdb/parser/peg/transformer/parse_result.hpp"

namespace duckdb {

//! Connect-mode (Layer 1) grammar — see connect_mode.gram for the canonical declaration.
//! Duplicated here as a string literal so the parser is self-contained and doesn't depend on the
//! main grammar's inlining pipeline. Keep in sync with connect_mode.gram if you edit either.
static constexpr const char *CONNECT_MODE_GRAMMAR = R"(
Program <- Chunk (';' Chunk)* ';'?
Chunk <- ExecuteChunk / ConnectChunk / DisconnectChunk / ForbiddenConnect / ForbiddenDisconnect / RawChunk
ConnectChunk <- 'CONNECT' (Identifier / 'LOCAL' / StringLiteral)?
DisconnectChunk <- 'DISCONNECT'
ExecuteChunk <- 'CONNECT' Identifier 'EXECUTE' Raw
ForbiddenConnect <- 'CONNECT' Raw
ForbiddenDisconnect <- 'DISCONNECT' Raw
RawChunk <- Raw
Raw <- InStatementToken+
)";
// Identifier and StringLiteral are provided as built-in overrides by
// MatcherFactory::CreateMatcherNoOverrides — no need to define them in the grammar.

//! Lazily-compiled, shared across all ConnectModeParser instances. Grammar compilation isn't
//! free (parses the grammar string into a matcher tree), so we cache it for the process lifetime.
static shared_ptr<PEGMatcher> GetConnectModeMatcher() {
	static shared_ptr<PEGMatcher> cached = PEGMatcher::CompileGrammar(CONNECT_MODE_GRAMMAR, "Program");
	return cached;
}

ConnectModeParser::ConnectModeParser(const string &sql_p)
    : sql(sql_p), matcher(GetConnectModeMatcher()), result_allocator(make_uniq<ParseResultAllocator>()) {
	// Use ParserTokenizer (not BaseTokenizer) so unquoted `;` is emitted as a TERMINATOR token —
	// the grammar relies on it to separate statements.
	ParserTokenizer tokenizer(sql, tokens);
	tokenizer.TokenizeInput();
	if (tokens.empty()) {
		return;
	}
	vector<MatcherSuggestion> suggestions;
	idx_t max_token_index = 0;
	MatchState state(tokens, suggestions, *result_allocator, max_token_index, /* preserve_identifier_case */ true);
	auto matched = matcher->Root().MatchParseResult(state);
	fprintf(stderr, "[CMP \"%s\"] tokens=%zu matched=%s state.token_index=%zu\n", sql.c_str(), tokens.size(),
	        matched ? "yes" : "null", state.token_index);
	if (matched) {
		fprintf(stderr, "%s\n", matched->ToString().c_str());
	}
	if (!matched || state.token_index < state.tokens.size()) {
		return;
	}
	program = matched->Cast<ListParseResult>();
}

ConnectModeParser::~ConnectModeParser() = default;

//! Recover the slice of the original SQL that a ParseResult covers, based on token offsets.
static string ExtractText(const string &sql, const vector<MatcherToken> &tokens, ParseResult &result) {
	// The simplest reliable signal we have is the offset on the leftmost-and-rightmost descendant
	// tokens. For v0 we approximate by walking via the structured types we know about.
	// TODO: replace with a real offset-range walker once we've built the broader implementation.
	(void)sql;
	(void)tokens;
	(void)result;
	return string();
}

//! Classify a Chunk ChoiceParseResult by inspecting the rule name of the alternative that matched.
static void ClassifyChunk(const string &sql, const vector<MatcherToken> &tokens, ParseResult &matched_alt,
                          ConnectModeChunk &out) {
	const auto &name = matched_alt.name;
	if (name == "ConnectChunk" || name == "DisconnectChunk") {
		out.type = ConnectModeChunk::Type::CONTROL;
	} else if (name == "ExecuteChunk") {
		out.type = ConnectModeChunk::Type::EXECUTE;
		// TODO: extract target and payload from the matched_alt's children.
	} else if (name == "ForbiddenConnect" || name == "ForbiddenDisconnect") {
		out.type = ConnectModeChunk::Type::FORBIDDEN;
	} else {
		out.type = ConnectModeChunk::Type::RAW;
	}
	out.text = ExtractText(sql, tokens, matched_alt);
}

bool ConnectModeParser::TryGetNext(ConnectModeChunk &out_chunk) {
	if (!program) {
		return false;
	}
	// Program <- Chunk (Semi Chunk)* Semi?
	// The ListParseResult holds the 3 sub-results. The (Semi Chunk)* portion is a RepeatParseResult.
	// We "linearize" the cursor by treating positions 0, 1, 2, ... as logical chunk indices and
	// fetching the corresponding Chunk node from the structured tree.
	(void)out_chunk;
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
