//===----------------------------------------------------------------------===//
//                         DuckDB
//
// tpch_extension.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"
namespace duckdb {

class TpchExtension : public NamedExtension<TpchExtension> {
public:
	static constexpr const char *name {"tpch"};

	void Load(DuckDB &db) override;

	//! Gets the specified TPC-H Query number as a string
	static std::string GetQuery(int query);
	//! Returns the CSV answer of a TPC-H query
	static std::string GetAnswer(double sf, int query);
};

} // namespace duckdb
