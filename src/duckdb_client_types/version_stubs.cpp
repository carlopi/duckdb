// Stub version info for wire mode — will be replaced with server-provided values
#include "duckdb.hpp"

namespace duckdb {

const char *DuckDB::SourceID() {
	return "wire-mode";
}

const char *DuckDB::LibraryVersion() {
	return "wire-mode";
}

const char *DuckDB::ReleaseCodename() {
	return "Wire Mode (Development)";
}

} // namespace duckdb
