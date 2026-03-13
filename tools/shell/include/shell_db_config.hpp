//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_db_config.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/case_insensitive_map.hpp"

#ifndef DUCKDB_SHELL_WIRE_MODE
namespace duckdb {
class DBConfig;
}
#endif

namespace duckdb_shell {
using duckdb::string;
using duckdb::unique_ptr;

//! Lightweight stand-in for DBConfig in wire mode.
//! Accepts the same calls but performs no validation (validation is server-side).
class MockDBConfig {
public:
	MockDBConfig();
	~MockDBConfig();
	void SetOptionByName(const string &, const duckdb::Value &) {
	}
	void SetSerializationCompatibility(const string &) {
	}
	// void AddCustomError(duckdb::ErrorType, const string &) {
	// }
	struct {
		duckdb::AccessMode access_mode = duckdb::AccessMode::READ_WRITE;
	} options;
};

//! ShellDBConfig accumulates configuration before the database is opened.
//! Stores options generically so it works in both local and wire mode.
//! In local mode, options are also validated eagerly against a real DBConfig.
//! In wire mode, a MockDBConfig is used (no validation).
class ShellDBConfig {
public:
	ShellDBConfig();
	~ShellDBConfig();

	//! Access mode
	void SetAccessMode(duckdb::AccessMode mode);
	void ResetAccessMode();

	//! Set an option by name
	void SetOptionByName(const string &name, const duckdb::Value &value);

	//! Set serialization compatibility
	void SetSerializationCompatibility(const string &version);

	// TODO: AddCustomError - DBConfig uses error_manager->AddCustomError(), needs interface alignment
	// void AddCustomError(duckdb::ErrorType type, const string &message);

	//! Accessors for stored config (used by ShellDuckDBLocal to build DBConfig)
	const duckdb::case_insensitive_map_t<duckdb::Value> &GetOptions() const;
	duckdb::AccessMode GetAccessMode() const;
	const string &GetSerializationCompatibility() const;

	// struct CustomError {
	// 	duckdb::ErrorType type;
	// 	string message;
	// };
	// const duckdb::vector<CustomError> &GetCustomErrors() const;

private:
	duckdb::case_insensitive_map_t<duckdb::Value> options;
	duckdb::AccessMode access_mode = duckdb::AccessMode::READ_WRITE;
	string serialization_compatibility;
	// duckdb::vector<CustomError> custom_errors;

#ifndef DUCKDB_SHELL_WIRE_MODE
	unique_ptr<duckdb::DBConfig> validator;
#else
	unique_ptr<MockDBConfig> validator;
#endif
};

} // namespace duckdb_shell
