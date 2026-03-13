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

namespace duckdb_shell {
using duckdb::string;
using duckdb::unique_ptr;

//! ShellDBConfig wraps a duckdb::DBConfig, exposing only the methods the shell needs.
//! The config is accumulated before the database is opened, then consumed by ShellDuckDB.
class ShellDBConfig {
#ifndef DUCKDB_SHELL_WIRE_MODE
	duckdb::DBConfig config;
#endif

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

	//! Add a custom error message
	void AddCustomError(duckdb::ErrorType type, const string &message);

#ifndef DUCKDB_SHELL_WIRE_MODE
	//! Get the underlying config for passing to ShellDuckDB at open time
	duckdb::DBConfig &GetNative();
#endif
};

} // namespace duckdb_shell
