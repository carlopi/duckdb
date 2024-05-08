//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_extension.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"

namespace duckdb {

class ShellExtension : public NamedExtension<ShellExtension> {
public:
	static constexpr const char *name {"shell"};

	void Load(DuckDB &db) override;
};

} // namespace duckdb
