#pragma once

#include "duckdb.hpp"

namespace duckdb {

class HttpfsExtension : public NamedExtension<HttpfsExtension> {
public:
	static constexpr const char *name {"httpfs"};

	void Load(DuckDB &db) override;
};

} // namespace duckdb
