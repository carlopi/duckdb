//===----------------------------------------------------------------------===//
//                         DuckDB
//
// inet_extension.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {

class InetExtension : public NamedExtension<InetExtension> {
public:
	static constexpr const char *name {"inet"};

	void Load(DuckDB &db) override;
};

} // namespace duckdb
