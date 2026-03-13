//===----------------------------------------------------------------------===//
//                         DuckDB
//
// logical_type_properties.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/string.hpp"

namespace duckdb {
class LogicalType;
} // namespace duckdb

namespace duckdb_shell {
using duckdb::string;

//! Lightweight type info for rendering — no engine dependency.
//! Carries only the display name and rendering-relevant flags.
struct LogicalTypeProperties {
	string name;
	bool is_numeric = false;
	bool is_nested = false;
	bool is_json = false;
	bool is_boolean = false;

	bool IsNumeric() const {
		return is_numeric;
	}
	bool IsNested() const {
		return is_nested;
	}
	bool IsJSONType() const {
		return is_json;
	}
	bool IsBoolean() const {
		return is_boolean;
	}

	//! Construct from a real LogicalType (local mode only — requires engine)
	static LogicalTypeProperties FromLogicalType(const duckdb::LogicalType &type);
};

} // namespace duckdb_shell
