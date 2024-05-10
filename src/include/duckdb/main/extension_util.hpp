//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/main/extension_util.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/constants.hpp"
#include "duckdb/function/cast/cast_function_set.hpp"
#include "duckdb/catalog/catalog_entry/function_entry.hpp"
#include "duckdb/function/function_set.hpp"
#include "duckdb/main/secret/secret.hpp"

namespace duckdb {
struct CreateMacroInfo;
struct CreateCollationInfo;
class DatabaseInstance;

namespace extension_util_impl {
//! The ExtensionUtil class contains methods that are useful for extensions
class ExtensionUtil {
public:
	template <class Derived>
	friend class NamedExtension;
	//! Register a new scalar function - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, ScalarFunction function);
	//! Register a new scalar function set - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, ScalarFunctionSet function);
	//! Register a new aggregate function - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, AggregateFunction function);
	//! Register a new aggregate function set - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, AggregateFunctionSet function);
	//! Register a new table function - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, TableFunction function);
	//! Register a new table function set - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, TableFunctionSet function);
	//! Register a new pragma function - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, PragmaFunction function);
	//! Register a new pragma function set - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, PragmaFunctionSet function);

	//! Register a CreateSecretFunction
	static void RegisterFunction(DatabaseInstance &db, CreateSecretFunction function);

	//! Register a new copy function - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, CopyFunction function);
	//! Register a new macro function - throw an exception if the function already exists
	static optional_ptr<CatalogEntry> RegisterFunction(DatabaseInstance &db, CreateMacroInfo &info);

	//! Register a new collation
	static void RegisterCollation(DatabaseInstance &db, CreateCollationInfo &info);

	//! Returns a reference to the function in the catalog - throws an exception if it does not exist
	static ScalarFunctionCatalogEntry &GetFunction(DatabaseInstance &db, const string &name);
	static TableFunctionCatalogEntry &GetTableFunction(DatabaseInstance &db, const string &name);

	//! Add a function overload
	static void AddFunctionOverload(DatabaseInstance &db, ScalarFunction function);
	static void AddFunctionOverload(DatabaseInstance &db, ScalarFunctionSet function);
	static void AddFunctionOverload(DatabaseInstance &db, TableFunctionSet function);

	//! Registers a new type
	static void RegisterType(DatabaseInstance &db, string type_name, LogicalType type);

	//! Registers a new secret type
	static void RegisterSecretType(DatabaseInstance &db, SecretType secret_type);

	//! Registers a cast between two types
	static void RegisterCastFunction(DatabaseInstance &db, const LogicalType &source,
	                                            const LogicalType &target, BoundCastInfo function,
	                                            int64_t implicit_cast_cost = -1);
};
} // namespace extension_util_impl

class ExtensionUtil : public extension_util_impl::ExtensionUtil {
public:
};

class ExtensionUtilExplicit : public extension_util_impl::ExtensionUtil {
public:
};

} // namespace duckdb
