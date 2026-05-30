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
Chunk <- ExecuteChunk / ForbiddenConnect1 / ConnectChunk / ForbiddenConnect2 / ForbiddenDisconnect / DisconnectChunk / RawChunk
ConnectChunk <- 'CONNECT' (Identifier / 'LOCAL' / StringLiteral)?
DisconnectChunk <- 'DISCONNECT'
ExecuteChunk <- 'CONNECT' Identifier 'EXECUTE' Raw
ForbiddenConnect1 <- 'CONNECT' (Identifier / 'LOCAL' / StringLiteral) Raw
ForbiddenConnect2 <- 'CONNECT' Raw
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
	if (!matched || state.token_index < state.tokens.size()) {
		return;
	}
	program = matched->Cast<ListParseResult>();
}

ConnectModeParser::~ConnectModeParser() = default;

//! Classify a Chunk LIST by inspecting the underlying CHOICE-resolved rule name.
//! The Chunk node has shape: LIST (Chunk) → CHOICE → LIST (ConnectChunk / DisconnectChunk / ...)
static void ClassifyChunk(ParseResult &chunk_node, ConnectModeChunk &out) {
	auto &chunk_list = chunk_node.Cast<ListParseResult>();
	auto &inner = chunk_list.GetChild(0);
	// The CHOICE wrapper exposes its resolved alternative via GetResult().
	auto &resolved = inner.Cast<ChoiceParseResult>().GetResult();
	const auto &name = resolved.name;
	if (name == "ConnectChunk" || name == "DisconnectChunk") {
		out.type = ConnectModeChunk::Type::CONTROL;
	} else if (name == "ExecuteChunk") {
		out.type = ConnectModeChunk::Type::EXECUTE;
		// TODO: extract target and payload from the resolved node's children.
	} else if (name == "ForbiddenConnect1" || name == "ForbiddenConnect2" || name == "ForbiddenDisconnect") {
		out.type = ConnectModeChunk::Type::FORBIDDEN;
	} else {
		// "RawChunk" or anything else: treat as raw.
		out.type = ConnectModeChunk::Type::RAW;
	}
	// TODO: populate out.text by slicing the original SQL using the resolved node's token offsets.
	out.text = string();
}

bool ConnectModeParser::TryGetNext(ConnectModeChunk &out_chunk) {
	if (!program) {
		return false;
	}
	// Program shape:
	//   child[0] = first Chunk
	//   child[1] = RepeatParseResult over LIST [Semi, Chunk] pairs ← additional chunks
	//   child[2] = OptionalParseResult for trailing Semi (always empty or contains ";")
	// Linearize the cursor: position 0 = first chunk, positions 1..N = repeat[i-1]'s second child.
	if (cursor == 0) {
		auto &first_chunk = program->GetChild(0);
		ClassifyChunk(first_chunk, out_chunk);
		cursor++;
		return true;
	}
	auto &repeat_node = program->GetChild(1);
	if (repeat_node.type == ParseResultType::REPEAT) {
		auto &repeat = repeat_node.Cast<RepeatParseResult>();
		auto children = repeat.GetChildren();
		idx_t repeat_idx = cursor - 1;
		if (repeat_idx < children.size()) {
			auto &pair = children[repeat_idx].get().Cast<ListParseResult>();
			auto &chunk_node = pair.GetChild(1);
			ClassifyChunk(chunk_node, out_chunk);
			cursor++;
			return true;
		}
	}
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
