#include "duckdb/main/capi/capi_internal.hpp"
#include <iostream>

using duckdb::Connection;
using duckdb::DatabaseData;
using duckdb::DBConfig;
using duckdb::DuckDB;
using duckdb::ErrorData;


duckdb_state duckdb_open_ext(const char *path, duckdb_database *out, duckdb_config config, char **error) {
	std::cout << "duckdb_open_ext 1\n";
	auto wrapper = new DatabaseData();
	std::cout << "duckdb_open_ext 2\n";
	try {
	std::cout << "duckdb_open_ext 3\n";
		DBConfig default_config;
	std::cout << "duckdb_open_ext 4\n";
		default_config.SetOptionByName("duckdb_api", "capi");
	std::cout << "duckdb_open_ext 5\n";

		DBConfig *db_config = &default_config;
	std::cout << "duckdb_open_ext 6\n";
		DBConfig *user_config = (DBConfig *)config;
	std::cout << "duckdb_open_ext 7\n";
		if (user_config) {
	std::cout << "duckdb_open_ext 8\n";
			db_config = user_config;
		}
	std::cout << "duckdb_open_ext 9\n";

		wrapper->database = duckdb::make_uniq<DuckDB>(path, db_config);
	std::cout << "duckdb_open_ext 10\n";
	} catch (std::exception &ex) {
	std::cout << "duckdb_open_ext 11\n";
		if (error) {
			ErrorData parsed_error(ex);
			*error = strdup(parsed_error.Message().c_str());
		}
	std::cout << "duckdb_open_ext 12\n";
		delete wrapper;
	std::cout << "duckdb_open_ext 13\n";
		return DuckDBError;
	} catch (...) { // LCOV_EXCL_START
	std::cout << "duckdb_open_ext 14\n";
		if (error) {
			*error = strdup("Unknown error");
		}
	std::cout << "duckdb_open_ext 15\n";
		delete wrapper;
	std::cout << "duckdb_open_ext 16\n";
		return DuckDBError;
	} // LCOV_EXCL_STOP
	std::cout << "duckdb_open_ext 17\n";
	*out = (duckdb_database)wrapper;
	std::cout << "duckdb_open_ext 18\n";
	return DuckDBSuccess;
}

duckdb_state duckdb_open(const char *path, duckdb_database *out) {
	return duckdb_open_ext(path, out, nullptr, nullptr);
}

void duckdb_close(duckdb_database *database) {
	if (database && *database) {
		auto wrapper = reinterpret_cast<DatabaseData *>(*database);
		delete wrapper;
		*database = nullptr;
	}
}

duckdb_state duckdb_connect(duckdb_database database, duckdb_connection *out) {
	if (!database || !out) {
		return DuckDBError;
	}
	auto wrapper = reinterpret_cast<DatabaseData *>(database);
	Connection *connection;
	try {
		connection = new Connection(*wrapper->database);
	} catch (...) { // LCOV_EXCL_START
		return DuckDBError;
	} // LCOV_EXCL_STOP
	*out = (duckdb_connection)connection;
	return DuckDBSuccess;
}

void duckdb_interrupt(duckdb_connection connection) {
	if (!connection) {
		return;
	}
	Connection *conn = reinterpret_cast<Connection *>(connection);
	conn->Interrupt();
}

duckdb_query_progress_type duckdb_query_progress(duckdb_connection connection) {
	duckdb_query_progress_type query_progress_type;
	query_progress_type.percentage = -1;
	query_progress_type.total_rows_to_process = 0;
	query_progress_type.rows_processed = 0;
	if (!connection) {
		return query_progress_type;
	}
	Connection *conn = reinterpret_cast<Connection *>(connection);
	auto query_progress = conn->context->GetQueryProgress();
	query_progress_type.total_rows_to_process = query_progress.GetTotalRowsToProcess();
	query_progress_type.rows_processed = query_progress.GetRowsProcesseed();
	query_progress_type.percentage = query_progress.GetPercentage();
	return query_progress_type;
}

void duckdb_disconnect(duckdb_connection *connection) {
	if (connection && *connection) {
		Connection *conn = reinterpret_cast<Connection *>(*connection);
		delete conn;
		*connection = nullptr;
	}
}

duckdb_state duckdb_query(duckdb_connection connection, const char *query, duckdb_result *out) {
	Connection *conn = reinterpret_cast<Connection *>(connection);
	auto result = conn->Query(query);
	return DuckDBTranslateResult(std::move(result), out);
}

const char *duckdb_library_version() {
	return DuckDB::LibraryVersion();
}
