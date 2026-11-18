#include "catch.hpp"
#include "test_helpers.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/transaction/duck_transaction_manager.hpp"
#include "duckdb/catalog/duck_catalog.hpp"

using namespace duckdb;

// A storage extension whose attach registers another storage extension under its own name, as a stand-in that loads
// the extension really serving the type does: the registry drops the stand-in during the attach, while core still
// has to call its create_transaction_manager. The attached database owns the storage extension it attaches through,
// so the stand-in outlives its registry entry.

static idx_t stand_in_attaches = 0;
static idx_t stand_in_transaction_managers = 0;
static idx_t real_attaches = 0;
static idx_t real_transaction_managers = 0;

struct RealStorageExtension : StorageExtension {
	RealStorageExtension() {
		attach = [](optional_ptr<StorageExtensionInfo>, ClientContext &, AttachedDatabase &db, const string &,
		            AttachInfo &, AttachOptions &) -> unique_ptr<Catalog> {
			real_attaches++;
			return make_uniq_base<Catalog, DuckCatalog>(db);
		};
		create_transaction_manager = [](optional_ptr<StorageExtensionInfo>, AttachedDatabase &db,
		                                Catalog &) -> unique_ptr<TransactionManager> {
			real_transaction_managers++;
			return make_uniq<DuckTransactionManager>(db);
		};
	}
};

struct StandInStorageExtension : StorageExtension {
	StandInStorageExtension() {
		attach = [](optional_ptr<StorageExtensionInfo>, ClientContext &context, AttachedDatabase &db, const string &,
		            AttachInfo &, AttachOptions &) -> unique_ptr<Catalog> {
			stand_in_attaches++;
			// what loading the real extension does: the stand-in's registry entry is replaced, and it is freed
			// unless the attach in progress owns it
			auto &config = DBConfig::GetConfig(DatabaseInstance::GetDatabase(context));
			StorageExtension::Register(config, "replaced", make_shared_ptr<RealStorageExtension>());
			return make_uniq_base<Catalog, DuckCatalog>(db);
		};
		create_transaction_manager = [](optional_ptr<StorageExtensionInfo>, AttachedDatabase &db,
		                                Catalog &) -> unique_ptr<TransactionManager> {
			stand_in_transaction_managers++;
			return make_uniq<DuckTransactionManager>(db);
		};
	}
};

TEST_CASE("Test storage extension replaced while attaching through it", "[api]") {
	stand_in_attaches = stand_in_transaction_managers = real_attaches = real_transaction_managers = 0;
	DBConfig config;
	StorageExtension::Register(config, "replaced", make_shared_ptr<StandInStorageExtension>());

	DuckDB db(nullptr, &config);
	Connection con(db);

	// the attach goes through the stand-in, which replaces itself; core then still calls the stand-in's
	// create_transaction_manager
	REQUIRE_NO_FAIL(con.Query("ATTACH ':memory:' AS db1 (TYPE replaced)"));
	REQUIRE(stand_in_attaches == 1);
	REQUIRE(stand_in_transaction_managers == 1);
	REQUIRE(real_attaches == 0);
	REQUIRE(real_transaction_managers == 0);
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE db1.t AS SELECT 42 AS i"));
	REQUIRE_NO_FAIL(con.Query("SELECT * FROM db1.t"));

	// from now on the type is the one registered during the attach
	REQUIRE_NO_FAIL(con.Query("ATTACH ':memory:' AS db2 (TYPE replaced)"));
	REQUIRE(stand_in_attaches == 1);
	REQUIRE(real_attaches == 1);
	REQUIRE(real_transaction_managers == 1);

	// the first database still works through the stand-in it was attached with
	REQUIRE_NO_FAIL(con.Query("SELECT * FROM db1.t"));
	REQUIRE_NO_FAIL(con.Query("DETACH db1"));
	REQUIRE_NO_FAIL(con.Query("DETACH db2"));
}
