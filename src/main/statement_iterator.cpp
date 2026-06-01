#include "duckdb/main/statement_iterator.hpp"

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/exception/parser_exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/database_manager.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/star_expression.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"

namespace duckdb {

namespace {

//! Wrap a TableRef returned from Catalog::RemoteExecute into `SELECT * FROM <ref>`. Identical to
//! the helper used inside the CONNECT chokepoint; duplicated here (in an anonymous namespace) so
//! the unity build doesn't see two `static` definitions of the same name across translation units.
unique_ptr<SQLStatement> WrapPassthroughAsSelect(unique_ptr<TableRef> from_ref) {
	auto select_node = make_uniq<SelectNode>();
	select_node->select_list.push_back(make_uniq<StarExpression>());
	select_node->from_table = std::move(from_ref);
	auto select_stmt = make_uniq<SelectStatement>();
	select_stmt->node = std::move(select_node);
	return std::move(select_stmt);
}

} // namespace

StatementIterator::StatementIterator(ClientContext &context_p, const string &sql) : context(context_p), layer1(sql) {
}

StatementIterator::~StatementIterator() = default;

StatementIterator::StatementKind StatementIterator::PeekKind() {
	if (buffer_pos < buffer.size()) {
		return current_kind;
	}
	EnsureBufferFilled();
	return current_kind;
}

unique_ptr<SQLStatement> StatementIterator::GetNext(optional_ptr<AttachedDatabase> &out_connect_target) {
	if (buffer_pos >= buffer.size()) {
		EnsureBufferFilled();
	}
	if (current_kind == StatementKind::NONE) {
		out_connect_target = nullptr;
		return nullptr;
	}
	if (current_kind == StatementKind::FORBIDDEN) {
		// Reset state so a subsequent PeekKind() returns NONE (don't re-throw forever).
		auto msg = std::move(forbidden_message);
		current_kind = StatementKind::NONE;
		throw ParserException(msg);
	}
	auto stmt = std::move(buffer[buffer_pos]);
	buffer_pos++;
	// PASSTHROUGH is the only kind that carries a target; LOCAL and CONTROL run unwrapped.
	out_connect_target = (current_kind == StatementKind::PASSTHROUGH) ? passthrough_target.get() : nullptr;
	return stmt;
}

void StatementIterator::EnsureBufferFilled() {
	// If we still have buffered statements, nothing to do. Otherwise, advance Layer 1 by one
	// chunk and translate it into statement(s) using the *current* binding state.
	if (buffer_pos < buffer.size()) {
		return;
	}
	buffer.clear();
	buffer_pos = 0;
	passthrough_target.reset();

	ConnectModeChunk chunk;
	if (!layer1.TryGetNext(chunk)) {
		current_kind = StatementKind::NONE;
		return;
	}

	switch (chunk.type) {
	case ConnectModeChunk::Type::FORBIDDEN: {
		current_kind = StatementKind::FORBIDDEN;
		// Match the per-shape error messages emitted by ClientContext::Query so callers see the
		// same diagnostic regardless of which entry point invoked the iterator.
		switch (chunk.forbidden_reason) {
		case ConnectModeChunk::ForbiddenReason::EXTRA_TOKENS_AFTER_CONNECT_TARGET:
			forbidden_message = StringUtil::Format(
			    "CONNECT statement has extra tokens after the target. Expected: 'CONNECT <name>;', "
			    "'CONNECT LOCAL;', or 'CONNECT '<connection-string>';'. Got: \"%s\"",
			    chunk.text);
			break;
		case ConnectModeChunk::ForbiddenReason::INVALID_CONNECT_TARGET:
			forbidden_message = StringUtil::Format(
			    "CONNECT requires a target identifier, LOCAL, or '<connection-string>'. Got: \"%s\"", chunk.text);
			break;
		case ConnectModeChunk::ForbiddenReason::MISSING_CONNECT_TARGET:
			forbidden_message = "CONNECT requires a target identifier, LOCAL, or '<connection-string>' "
			                    "(none was given)";
			break;
		case ConnectModeChunk::ForbiddenReason::CONNECT_EXECUTE_MISSING_TARGET:
			forbidden_message = StringUtil::Format(
			    "CONNECT EXECUTE requires a target name. Use: 'CONNECT <name> EXECUTE <sql>'. Got: \"%s\"",
			    chunk.text);
			break;
		case ConnectModeChunk::ForbiddenReason::EXTRA_TOKENS_AFTER_DISCONNECT:
			forbidden_message = StringUtil::Format("DISCONNECT takes no arguments. Got: \"%s\"", chunk.text);
			break;
		default:
			forbidden_message = StringUtil::Format("Malformed CONNECT/DISCONNECT: \"%s\"", chunk.text);
			break;
		}
		return;
	}
	case ConnectModeChunk::Type::EXECUTE: {
		if (StringUtil::CIEquals(chunk.target, "LOCAL")) {
			// `CONNECT LOCAL EXECUTE <sql>` — run the payload locally, no wrap.
			auto stmts = context.ParseStatements(chunk.payload);
			for (auto &stmt : stmts) {
				buffer.push_back(std::move(stmt));
			}
			current_kind = StatementKind::LOCAL;
			return;
		}
		// `CONNECT <name> EXECUTE <sql>` — pre-wrap against the named target catalog.
		auto target_db = DatabaseManager::Get(context).GetDatabase(chunk.target);
		if (!target_db) {
			throw InvalidInputException("Database \"%s\" is not attached", chunk.target);
		}
		if (target_db->GetConnectMode() != ConnectMode::ENABLE) {
			throw InvalidInputException("Database \"%s\" does not support CONNECT", chunk.target);
		}
		auto remote_ref = target_db->GetCatalog().RemoteExecute(context, chunk.payload);
		auto stmt = WrapAsSelect(std::move(remote_ref));
		stmt->query = chunk.payload;
		buffer.push_back(std::move(stmt));
		passthrough_target = std::move(target_db);
		current_kind = StatementKind::PASSTHROUGH;
		return;
	}
	case ConnectModeChunk::Type::CONTROL: {
		auto stmts = context.ParseStatements(chunk.text);
		for (auto &stmt : stmts) {
			buffer.push_back(std::move(stmt));
		}
		current_kind = StatementKind::CONTROL;
		return;
	}
	case ConnectModeChunk::Type::RAW: {
		auto wrap_target = context.TryGetConnectWrapTarget();
		if (wrap_target) {
			// RAW + bound: pre-wrap against the current binding.
			auto remote_ref = wrap_target->GetCatalog().RemoteExecute(context, chunk.text);
			auto stmt = WrapAsSelect(std::move(remote_ref));
			stmt->query = chunk.text;
			buffer.push_back(std::move(stmt));
			passthrough_target = std::move(wrap_target);
			current_kind = StatementKind::PASSTHROUGH;
		} else {
			// RAW + unbound: parse via the SQL parser (may decompose into MultiStatement).
			auto stmts = context.ParseStatements(chunk.text);
			for (auto &stmt : stmts) {
				buffer.push_back(std::move(stmt));
			}
			current_kind = StatementKind::LOCAL;
		}
		return;
	}
	}
}

} // namespace duckdb
