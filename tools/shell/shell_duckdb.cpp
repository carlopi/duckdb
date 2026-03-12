#include "shell_duckdb_local.hpp"
#include "shell_db_config.hpp"
#include "shell_connection_local.hpp"

namespace duckdb_shell {

ShellDuckDBLocal::ShellDuckDBLocal(const char *path, ShellDBConfig &config)
    : db(make_uniq<duckdb::DuckDB>(path, &config.GetNative())) {
}

ShellDuckDBLocal::~ShellDuckDBLocal() {
}

bool ShellDuckDBLocal::IsOpen() const {
	return db != nullptr;
}

void ShellDuckDBLocal::Reset() {
	db.reset();
}

duckdb::DatabaseInstance &ShellDuckDBLocal::GetInstance() {
	return *db->instance;
}

unique_ptr<ShellConnection> ShellDuckDBLocal::CreateConnection() {
	return make_uniq<ShellConnectionLocal>(make_uniq<duckdb::Connection>(*db));
}

unique_ptr<ShellDuckDB> ShellDuckDB::Create(const char *path, ShellDBConfig &config) {
	return make_uniq<ShellDuckDBLocal>(path, config);
}

} // namespace duckdb_shell
