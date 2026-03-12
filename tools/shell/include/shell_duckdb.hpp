//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_duckdb.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/types.hpp"

namespace duckdb_shell {
using duckdb::unique_ptr;
class ShellConnection;
class ShellDBConfig;

//! ShellDuckDB is an abstract interface for a database instance.
class ShellDuckDB {
public:
	virtual ~ShellDuckDB() = default;

	//! Whether the database is open
	virtual bool IsOpen() const = 0;
	//! Close the database
	virtual void Reset() = 0;

	//! Create a new connection to the database (returns abstract base type)
	virtual unique_ptr<ShellConnection> CreateConnection() = 0;

	//! Factory: creates a ShellDuckDBLocal or ShellDuckDBWired depending on build mode
	static unique_ptr<ShellDuckDB> Create(const char *path, ShellDBConfig &config);
};

} // namespace duckdb_shell
