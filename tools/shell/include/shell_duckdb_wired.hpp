//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_duckdb_wired.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_duckdb.hpp"
#include "wire_transport.hpp"

namespace duckdb_shell {
class ShellDBConfig;

//! Wire-mode implementation of ShellDuckDB.
//! Owns a TransportLayer and delegates all operations through it.
class ShellDuckDBWired : public ShellDuckDB {
public:
	explicit ShellDuckDBWired(const char *path, ShellDBConfig &config);
	~ShellDuckDBWired() override;

	bool IsOpen() const override;
	void Reset() override;
	unique_ptr<ShellConnection> CreateConnection() override;

	//! Access the transport layer (for connections to use)
	TransportLayer &GetTransport();

private:
	unique_ptr<TransportLayer> transport;
	bool is_open = false;
};

} // namespace duckdb_shell
