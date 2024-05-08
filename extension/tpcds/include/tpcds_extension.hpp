//===----------------------------------------------------------------------===//
//                         DuckDB
//
// tpcds_extension.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {

class TpcdsExtension : public NamedExtension<TpcdsExtension> {
public:
	static constexpr const char *name {"tpcds"};

	void Load(DuckDB &db) override;

	//! Gets the specified TPC-DS Query number as a string
	static std::string GetQuery(int query);
	//! Returns the CSV answer of a TPC-DS query
	static std::string GetAnswer(double sf, int query);
};

} // namespace duckdb
