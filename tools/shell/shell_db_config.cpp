#include "shell_db_config.hpp"
#ifndef DUCKDB_SHELL_WIRE_MODE
#include "duckdb/main/error_manager.hpp"
#endif

namespace duckdb_shell {

ShellDBConfig::ShellDBConfig() {
}

ShellDBConfig::~ShellDBConfig() {
}

void ShellDBConfig::SetAccessMode(duckdb::AccessMode mode) {
#ifndef DUCKDB_SHELL_WIRE_MODE
	config.options.access_mode = mode;
#endif
}

void ShellDBConfig::ResetAccessMode() {
#ifndef DUCKDB_SHELL_WIRE_MODE
	config.options.access_mode = duckdb::AccessMode::READ_WRITE;
#endif
}

void ShellDBConfig::SetOptionByName(const string &name, const duckdb::Value &value) {
#ifndef DUCKDB_SHELL_WIRE_MODE
	config.SetOptionByName(name, value);
#endif
}

void ShellDBConfig::SetSerializationCompatibility(const string &version) {
#ifndef DUCKDB_SHELL_WIRE_MODE
	config.options.serialization_compatibility = duckdb::SerializationCompatibility::FromString(version);
#endif
}

void ShellDBConfig::AddCustomError(duckdb::ErrorType type, const string &message) {
#ifndef DUCKDB_SHELL_WIRE_MODE
	config.error_manager->AddCustomError(type, message);
#endif
}

#ifndef DUCKDB_SHELL_WIRE_MODE
duckdb::DBConfig &ShellDBConfig::GetNative() {
	return config;
}
#endif

} // namespace duckdb_shell
