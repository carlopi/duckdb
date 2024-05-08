//===----------------------------------------------------------------------===//
//                         DuckDB
//
// json_extension.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"

namespace duckdb {

class JsonExtension : public NamedExtension<JsonExtension> {
public:
	static constexpr const char *name {"json"};

	void Load(DuckDB &db) override;
};

} // namespace duckdb
