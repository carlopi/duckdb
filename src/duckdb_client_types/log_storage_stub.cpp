// Wire-mode stub for LogStorage — only base class methods.
// The subclasses (BufferingLogStorage, CSVLogStorage, FileLogStorage, etc.)
// are not needed in wire mode since logging is server-side.

#include "duckdb/parser/tableref.hpp"
#include "duckdb/logging/log_storage.hpp"

namespace duckdb {

vector<LogicalType> LogStorage::GetSchema(LoggingTargetTable table) {
	switch (table) {
	case LoggingTargetTable::ALL_LOGS: {
		auto all_logs = GetSchema(LoggingTargetTable::LOG_CONTEXTS);
		auto log_entries = GetSchema(LoggingTargetTable::LOG_ENTRIES);
		all_logs.insert(all_logs.end(), log_entries.begin() + 1, log_entries.end());
		return all_logs;
	}
	case LoggingTargetTable::LOG_ENTRIES:
		return {
		    LogicalType::UBIGINT,      // context_id
		    LogicalType::TIMESTAMP_TZ, // timestamp
		    LogicalType::VARCHAR,      // log_type
		    LogicalType::VARCHAR,      // level
		    LogicalType::VARCHAR,      // message
		};
	case LoggingTargetTable::LOG_CONTEXTS:
		return {
		    LogicalType::UBIGINT, // context_id
		    LogicalType::VARCHAR, // scope
		    LogicalType::UBIGINT, // connection_id
		    LogicalType::UBIGINT, // transaction_id
		    LogicalType::UBIGINT, // query_id
		    LogicalType::UBIGINT, // thread
		};
	default:
		throw NotImplementedException("Unknown logging target table");
	}
}

vector<string> LogStorage::GetColumnNames(LoggingTargetTable table) {
	switch (table) {
	case LoggingTargetTable::ALL_LOGS: {
		auto all_logs = GetColumnNames(LoggingTargetTable::LOG_CONTEXTS);
		auto log_entries = GetColumnNames(LoggingTargetTable::LOG_ENTRIES);
		all_logs.insert(all_logs.end(), log_entries.begin() + 1, log_entries.end());
		return all_logs;
	}
	case LoggingTargetTable::LOG_ENTRIES:
		return {"context_id", "timestamp", "type", "log_level", "message"};
	case LoggingTargetTable::LOG_CONTEXTS:
		return {
		    "context_id", "scope", "connection_id", "transaction_id", "query_id", "thread_id",
		};
	default:
		throw NotImplementedException("Unknown logging target table");
	}
}

unique_ptr<LogStorageScanState> LogStorage::CreateScanState(LoggingTargetTable table) const {
	throw NotImplementedException("Not implemented for this LogStorage: CreateScanEntriesState");
}

bool LogStorage::Scan(LogStorageScanState &state, DataChunk &result) const {
	throw NotImplementedException("Not implemented for this LogStorage: ScanEntries");
}

void LogStorage::InitializeScan(LogStorageScanState &state) const {
	throw NotImplementedException("Not implemented for this LogStorage: InitializeScanEntries");
}

void LogStorage::Truncate() {
	throw NotImplementedException("Not implemented for this LogStorage: TruncateLogStorage");
}

void LogStorage::UpdateConfig(DatabaseInstance &db, case_insensitive_map_t<Value> &config) {
	if (config.size() > 1) {
		throw InvalidInputException("LogStorage does not support passing configuration");
	}
}

unique_ptr<TableRef> LogStorage::BindReplace(ClientContext &context, TableFunctionBindInput &input,
                                             LoggingTargetTable table) {
	return nullptr;
}

} // namespace duckdb
