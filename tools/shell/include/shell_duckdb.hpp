//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_duckdb.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"
#include "duckdb/common/unique_ptr.hpp"

namespace duckdb_shell {
using duckdb::make_uniq;
using duckdb::string;
using duckdb::unique_ptr;

//! ShellDuckDB wraps a duckdb::DuckDB instance, exposing only the methods the shell needs.
//! No virtual methods yet — polymorphism comes later when wire mode lands.
class ShellDuckDB {
	unique_ptr<duckdb::DuckDB> db;

public:
	explicit ShellDuckDB(const char *path, duckdb::DBConfig *config);
	~ShellDuckDB();

	//! Whether the database is open
	bool IsOpen() const;
	//! Close the database
	void Reset();

	//! Get the underlying database instance
	duckdb::DatabaseInstance &GetInstance();

	//! Create a new connection to the database
	unique_ptr<duckdb::Connection> CreateConnection();

	//! Load a statically-linked extension
	template <class T>
	void LoadStaticExtension() {
		db->LoadStaticExtension<T>();
	}
};

} // namespace duckdb_shell
