//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/main/extension.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/winapi.hpp"
#include "duckdb/catalog/catalog_entry/function_entry.hpp"
#include "duckdb/main/extension_util.hpp"

namespace duckdb {
class DuckDB;

//! The Extension class is the base class used to define extensions
class Extension {
public:
	DUCKDB_API virtual ~Extension();

	DUCKDB_API virtual void Load(DuckDB &db) = 0;
	DUCKDB_API virtual std::string Name() = 0;
};

//! The NamedExtension class uses curiously recurring template pattern to have extension name known at compile time
template <class Derived>
class NamedExtension : public Extension {
public:
	DUCKDB_API virtual void Load(DuckDB &db) = 0;
	DUCKDB_API virtual std::string Name() {
		return Derived::name;
	}

	template <typename T>
	static void RegisterFunction(DatabaseInstance &db, T &&fun) {
		fun.SetExtension(Derived::name);
		ExtensionUtilExplicit::RegisterFunction(db, fun);
	}
	static void RegisterFunction(DatabaseInstance &db, CreateSecretFunction fun) {
		// TODO: enable registration of secrets functions
		ExtensionUtilExplicit::RegisterFunction(db, fun);
	}
	template <typename T>
	static void AddFunctionOverload(DatabaseInstance &db, T fun) {
		fun.SetExtension(Derived::name);
		ExtensionUtilExplicit::AddFunctionOverload(db, fun);
	}
	static void RegisterType(DatabaseInstance &db, string type_name, LogicalType type) {
		ExtensionUtilExplicit::RegisterType(db, type_name, type);
	}
	static void RegisterSecretType(DatabaseInstance &db, SecretType secret_type) {
		// TODO: enable registration of secrets types
		ExtensionUtilExplicit::RegisterSecretType(db, secret_type);
	}
	static void RegisterCastFunction(DatabaseInstance &db, const LogicalType &source, const LogicalType &target,
	                                 BoundCastInfo function, int64_t implicit_cast_cost = -1) {
		ExtensionUtilExplicit::RegisterCastFunction(db, source, target, std::move(function), implicit_cast_cost);
	}
};

} // namespace duckdb
