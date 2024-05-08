#pragma once

#include "duckdb.hpp"

namespace duckdb {

class ParquetExtension : public NamedExtension<ParquetExtension> {
public:
	static constexpr const char *name {"parquet"};

	void Load(DuckDB &db) override;
};

} // namespace duckdb
