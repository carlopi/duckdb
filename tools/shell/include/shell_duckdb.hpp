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
};

} // namespace duckdb_shell
