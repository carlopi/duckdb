#include "duckdb/execution/operator/helper/physical_connect.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/database_manager.hpp"
#include "duckdb/storage/storage_extension.hpp"

namespace duckdb {

SourceResultType PhysicalConnect::GetDataInternal(ExecutionContext &context, DataChunk &chunk,
                                                  OperatorSourceInput &input) const {
	auto &client = context.client;

	// `CONNECT LOCAL` — semantically a no-op when we're already LOCAL (no current binding). The
	// single-binding rule still applies: rejecting the statement while bound forces the user to
	// DISCONNECT first, which keeps the binding state transitions explicit.
	if (info->target_is_local) {
		if (client.IsConnected()) {
			auto current = client.TryGetConnectedCatalog();
			throw InvalidInputException("Already connected to \"%s\"; DISCONNECT first to switch to LOCAL",
			                            current ? current->GetName() : "<detached>");
		}
		return SourceResultType::FINISHED;
	}
	if (info->name.empty()) {
		// Layer 1 catches bare `CONNECT;` as MISSING_CONNECT_TARGET before this point — any remaining
		// path here is via Parser-only entry points that don't go through Query(string).
		throw NotImplementedException("CONNECT with no target is not yet implemented");
	}
	if (info->name_is_string_literal) {
		throw NotImplementedException("CONNECT '<connection_string>' is not yet implemented");
	}

	// At most one active connection; only DISCONNECT clears it (even if the target was detached
	// elsewhere, so the user explicitly acknowledges the broken connection).
	if (client.IsConnected()) {
		auto current = client.TryGetConnectedCatalog();
		throw InvalidInputException("Already connected to \"%s\"; DISCONNECT first before issuing another CONNECT",
		                            current ? current->GetName() : "<detached>");
	}

	auto target = DatabaseManager::Get(client).GetDatabase(info->name);
	if (!target) {
		throw InvalidInputException("Database \"%s\" is not attached", info->name);
	}
	// ConnectToCatalog resolves and caches the dispatch function name; throws if the catalog returns
	// empty from GetConnectFunctionName (i.e. CONNECT is not supported in this context).
	client.ConnectToCatalog(target);
	return SourceResultType::FINISHED;
}

} // namespace duckdb
