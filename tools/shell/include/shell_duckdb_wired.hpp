//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_duckdb_wired.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_duckdb.hpp"

namespace duckdb_shell {
class ShellDBConfig;

//! Wire-mode stub for ShellDuckDB.
class ShellDuckDBWired : public ShellDuckDB {
public:
	explicit ShellDuckDBWired(const char *path, ShellDBConfig &config);
	~ShellDuckDBWired() override;

	bool IsOpen() const override;
	void Reset() override;
	unique_ptr<ShellConnection> CreateConnection() override;
};

} // namespace duckdb_shell
