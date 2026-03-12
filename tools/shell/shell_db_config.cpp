#include "shell_db_config.hpp"
#include "duckdb/main/error_manager.hpp"

namespace duckdb_shell {

ShellDBConfig::ShellDBConfig() {
}

ShellDBConfig::~ShellDBConfig() {
}

void ShellDBConfig::SetAccessMode(duckdb::AccessMode mode) {
	config.options.access_mode = mode;
}

void ShellDBConfig::ResetAccessMode() {
	config.options.access_mode = duckdb::AccessMode::READ_WRITE;
}

void ShellDBConfig::SetOptionByName(const string &name, const duckdb::Value &value) {
	config.SetOptionByName(name, value);
}

void ShellDBConfig::SetSerializationCompatibility(const string &version) {
	config.options.serialization_compatibility = duckdb::SerializationCompatibility::FromString(version);
}

void ShellDBConfig::AddCustomError(duckdb::ErrorType type, const string &message) {
	config.error_manager->AddCustomError(type, message);
}

duckdb::DBConfig &ShellDBConfig::GetNative() {
	return config;
}

} // namespace duckdb_shell
