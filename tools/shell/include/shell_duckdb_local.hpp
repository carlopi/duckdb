//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_duckdb_local.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_duckdb.hpp"
#include "duckdb.hpp"

namespace duckdb_shell {
using duckdb::make_uniq;
using duckdb::string;
class ShellDBConfig;

//! ShellDuckDBLocal wraps a duckdb::DuckDB instance.
class ShellDuckDBLocal : public ShellDuckDB {
public:
	explicit ShellDuckDBLocal(const char *path, ShellDBConfig &config);
	~ShellDuckDBLocal() override;

	//! Base interface
	bool IsOpen() const override;
	void Reset() override;
	unique_ptr<ShellConnection> CreateConnection() override;

	//! Engine-only: get the underlying database instance
	duckdb::DatabaseInstance &GetInstance();

	//! Engine-only: load a statically-linked extension
	template <class T>
	void LoadStaticExtension() {
		db->LoadStaticExtension<T>();
	}

private:
	unique_ptr<duckdb::DuckDB> db;
};

} // namespace duckdb_shell
