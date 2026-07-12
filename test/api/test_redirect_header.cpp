#include "catch.hpp"
#include "test_helpers.hpp"
#include "duckdb/storage/storage_info.hpp"
#include "duckdb/storage/redirect_info.hpp"
#include "duckdb/common/serializer/memory_stream.hpp"

using namespace duckdb;

TEST_CASE("MainHeader redirect record round-trips through Write/Read", "[api][redirect]") {
	MainHeader header {};
	header.version_number = static_cast<idx_t>(StorageVersion::V1_4_0);
	header.SetRedirect();
	REQUIRE(header.IsRedirect());

	header.redirect.is_redirect = true;
	header.redirect.type = "iceberg";
	header.redirect.target = "s3://warehouse/table";
	header.redirect.options.push_back({"catalog", "glue"});
	header.redirect.options.push_back({"region", "us-east-1"});

	MemoryStream stream;
	header.Write(stream);
	stream.Rewind();

	auto read_header = MainHeader::Read(stream);
	REQUIRE(read_header.IsRedirect());
	REQUIRE(read_header.redirect.is_redirect);
	REQUIRE(read_header.redirect.version == RedirectInfo::CURRENT_VERSION);
	REQUIRE(read_header.redirect.type == "iceberg");
	REQUIRE(read_header.redirect.target == "s3://warehouse/table");
	REQUIRE(read_header.redirect.options.size() == 2);
	REQUIRE(read_header.redirect.options[0].key == "catalog");
	REQUIRE(read_header.redirect.options[0].value == "glue");
	REQUIRE(read_header.redirect.options[1].key == "region");
	REQUIRE(read_header.redirect.options[1].value == "us-east-1");
}

TEST_CASE("MainHeader without redirect flag reads no redirect record", "[api][redirect]") {
	MainHeader header {};
	header.version_number = static_cast<idx_t>(StorageVersion::V1_4_0);
	REQUIRE(!header.IsRedirect());

	MemoryStream stream;
	header.Write(stream);
	stream.Rewind();

	auto read_header = MainHeader::Read(stream);
	REQUIRE(!read_header.IsRedirect());
	REQUIRE(!read_header.redirect.is_redirect);
}

TEST_CASE("Redirect record with no options round-trips", "[api][redirect]") {
	RedirectInfo redirect;
	redirect.is_redirect = true;
	redirect.type = "";
	redirect.target = "../moved/real.db";

	MemoryStream stream;
	redirect.Serialize(stream);
	stream.Rewind();

	auto read = RedirectInfo::Deserialize(stream);
	REQUIRE(read.is_redirect);
	REQUIRE(read.type.empty());
	REQUIRE(read.target == "../moved/real.db");
	REQUIRE(read.options.empty());
}
