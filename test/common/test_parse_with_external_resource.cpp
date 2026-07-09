#include "catch.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/parsed_data/external_resource_options.hpp"
#include "duckdb/parser/statement/attach_statement.hpp"
#include "duckdb/parser/statement/connect_statement.hpp"
#include "test_helpers.hpp"

using namespace duckdb;

TEST_CASE("Parse WITH EXTERNAL RESOURCE ... ATTACH + ToString roundtrip", "[parse_external_resource]") {
	Parser parser;
	parser.ParseQuery("WITH EXTERNAL RESOURCE 'quack@local' AS beefy (INSTANCE 'r7i.16xlarge', REGION 'eu-west-1') "
	                  "ATTACH AS my_db (READ_ONLY)");
	REQUIRE(parser.statements.size() == 1);
	REQUIRE(parser.statements[0]->type == StatementType::ATTACH_STATEMENT);

	auto &attach = parser.statements[0]->Cast<AttachStatement>();
	REQUIRE(attach.info->external_resource != nullptr);
	auto &er = *attach.info->external_resource;
	// provider (type) is separated from the create params.
	REQUIRE(er.parsed_type != nullptr);
	REQUIRE(StringUtil::Contains(er.parsed_type->ToString(), "quack@local"));
	REQUIRE(er.alias.GetIdentifierName() == "beefy");
	REQUIRE(er.parsed_params.size() == 2);
	REQUIRE(er.parsed_params.find("INSTANCE") != er.parsed_params.end());
	REQUIRE(er.parsed_params.find("REGION") != er.parsed_params.end());
	// the attach alias is the (functional) database name, distinct from the resource alias.
	REQUIRE(attach.info->name.GetIdentifierName() == "my_db");

	// ToString renders back to the WITH EXTERNAL RESOURCE surface (not the lowered ATTACH form).
	auto str = attach.info->ToString();
	REQUIRE(StringUtil::Contains(str, "WITH EXTERNAL RESOURCE"));
	REQUIRE(StringUtil::Contains(str, "quack@local"));
	REQUIRE(StringUtil::Contains(str, "AS beefy"));
	REQUIRE(StringUtil::Contains(str, "ATTACH"));
	REQUIRE(StringUtil::Contains(str, "AS my_db"));

	// Roundtrip: re-parsing the rendered SQL yields the same statement + resource.
	Parser reparser;
	reparser.ParseQuery(str);
	REQUIRE(reparser.statements.size() == 1);
	REQUIRE(reparser.statements[0]->type == StatementType::ATTACH_STATEMENT);
	auto &re = reparser.statements[0]->Cast<AttachStatement>();
	REQUIRE(re.info->external_resource != nullptr);
	REQUIRE(re.info->external_resource->alias.GetIdentifierName() == "beefy");
	REQUIRE(re.info->external_resource->parsed_params.size() == 2);
	REQUIRE(re.info->name.GetIdentifierName() == "my_db");
	REQUIRE(StringUtil::Contains(re.info->ToString(), "WITH EXTERNAL RESOURCE"));
	// (Exact string idempotence is covered by the single-param CONNECT case below; multi-param
	// option ordering is intentionally unspecified — it rides the same unordered options map as ATTACH.)
}

TEST_CASE("Parse WITH EXTERNAL RESOURCE ... CONNECT + ToString roundtrip", "[parse_external_resource]") {
	Parser parser;
	parser.ParseQuery("WITH EXTERNAL RESOURCE 'quack@local' (region 'eu-west-1') CONNECT (token 'abc')");
	REQUIRE(parser.statements.size() == 1);
	REQUIRE(parser.statements[0]->type == StatementType::CONNECT_STATEMENT);

	auto &connect = parser.statements[0]->Cast<ConnectStatement>();
	REQUIRE(connect.info->external_resource != nullptr);
	REQUIRE(connect.info->external_resource->parsed_params.size() == 1);

	auto str = connect.info->ToString();
	REQUIRE(StringUtil::Contains(str, "WITH EXTERNAL RESOURCE"));
	REQUIRE(StringUtil::Contains(str, "quack@local"));
	REQUIRE(StringUtil::Contains(str, "CONNECT"));

	Parser reparser;
	reparser.ParseQuery(str);
	REQUIRE(reparser.statements.size() == 1);
	REQUIRE(reparser.statements[0]->type == StatementType::CONNECT_STATEMENT);
	REQUIRE(reparser.statements[0]->Cast<ConnectStatement>().info->external_resource != nullptr);
	REQUIRE(reparser.statements[0]->Cast<ConnectStatement>().info->ToString() == str);
}
