#include "shell_duckdb_local.hpp"
#include "shell_db_config.hpp"
#include "shell_connection_local.hpp"
#include "duckdb/main/error_manager.hpp"

namespace duckdb_shell {

ShellDuckDBLocal::ShellDuckDBLocal(const char *path, ShellDBConfig &config) {
	duckdb::DBConfig db_config;
	db_config.options.access_mode = config.GetAccessMode();
	for (auto &option : config.GetOptions()) {
		db_config.SetOptionByName(option.first, option.second);
	}
	auto &compat = config.GetSerializationCompatibility();
	if (!compat.empty()) {
		db_config.SetSerializationCompatibility(compat);
	}
	for (auto &error : config.GetCustomErrors()) {
		db_config.AddCustomError(error.type, error.message);
	}
	db = make_uniq<duckdb::DuckDB>(path, &db_config);
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

#if !defined(DUCKDB_SHELL_WIRE_MODE) && !defined(DUCKDB_SHELL_WIRE_TEST)
unique_ptr<ShellDuckDB> ShellDuckDB::Create(const char *path, ShellDBConfig &config) {
	return make_uniq<ShellDuckDBLocal>(path, config);
}
#endif

} // namespace duckdb_shell
