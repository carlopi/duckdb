#include "shell_db_config.hpp"
#ifndef DUCKDB_SHELL_WIRE_MODE
#include "duckdb/main/config.hpp"
#include "duckdb/main/error_manager.hpp"
#endif

namespace duckdb_shell {

MockDBConfig::MockDBConfig() {
}

MockDBConfig::~MockDBConfig() {
}

ShellDBConfig::ShellDBConfig() {
#ifndef DUCKDB_SHELL_WIRE_MODE
	validator = duckdb::make_uniq<duckdb::DBConfig>();
#else
	validator = duckdb::make_uniq<MockDBConfig>();
#endif
}

ShellDBConfig::~ShellDBConfig() {
}

void ShellDBConfig::SetAccessMode(duckdb::AccessMode mode) {
	access_mode = mode;
	validator->options.access_mode = mode;
}

void ShellDBConfig::ResetAccessMode() {
	access_mode = duckdb::AccessMode::READ_WRITE;
	validator->options.access_mode = duckdb::AccessMode::READ_WRITE;
}

void ShellDBConfig::SetOptionByName(const string &name, const duckdb::Value &value) {
	validator->SetOptionByName(name, value);
	options[name] = value;
}

void ShellDBConfig::SetSerializationCompatibility(const string &version) {
	validator->SetSerializationCompatibility(version);
	serialization_compatibility = version;
}

void ShellDBConfig::AddCustomError(duckdb::ErrorType type, const string &message) {
	validator->AddCustomError(type, message);
	custom_errors.push_back({type, message});
}

const duckdb::case_insensitive_map_t<duckdb::Value> &ShellDBConfig::GetOptions() const {
	return options;
}

duckdb::AccessMode ShellDBConfig::GetAccessMode() const {
	return access_mode;
}

const string &ShellDBConfig::GetSerializationCompatibility() const {
	return serialization_compatibility;
}

const duckdb::vector<ShellDBConfig::CustomError> &ShellDBConfig::GetCustomErrors() const {
	return custom_errors;
}

} // namespace duckdb_shell
