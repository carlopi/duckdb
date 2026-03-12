#include "shell_duckdb.hpp"
#include "shell_db_config.hpp"

namespace duckdb_shell {

ShellDuckDB::ShellDuckDB(const char *path, ShellDBConfig &config)
    : db(make_uniq<duckdb::DuckDB>(path, &config.GetNative())) {
}

ShellDuckDB::~ShellDuckDB() {
}

bool ShellDuckDB::IsOpen() const {
	return db != nullptr;
}

void ShellDuckDB::Reset() {
	db.reset();
}

duckdb::DatabaseInstance &ShellDuckDB::GetInstance() {
	return *db->instance;
}

unique_ptr<duckdb::Connection> ShellDuckDB::CreateConnection() {
	return make_uniq<duckdb::Connection>(*db);
}

} // namespace duckdb_shell
