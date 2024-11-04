//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/main/serialization_compatibility.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/string.hpp"

namespace duckdb {

class SerializationCompatibility {
public:
	static SerializationCompatibility FromString(const string &input);
	static SerializationCompatibility Default();
	static SerializationCompatibility Latest();

public:
	bool Compare(idx_t property_version) const;

public:
	//! The user provided version
	string duckdb_version;
	//! The max version that should be serialized
	idx_t serialization_version;
	//! Whether this was set by a manual SET/PRAGMA or default
	bool manually_set;

protected:
	SerializationCompatibility() = default;
};

} // namespace duckdb
