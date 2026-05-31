#include "catch.hpp"

#include "duckdb/parser/connect_mode/connect_mode_parser.hpp"

using namespace duckdb; // NOLINT

TEST_CASE("ConnectModeParser: empty / whitespace input yields no chunks", "[connect_mode]") {
	{
		ConnectModeParser p("");
		REQUIRE(p.AllRemaining().empty());
	}
	{
		ConnectModeParser p("   \t\n  ");
		REQUIRE(p.AllRemaining().empty());
	}
	{
		ConnectModeParser p(";");
		REQUIRE(p.AllRemaining().empty());
	}
	{
		// A run of bare semicolons is a valid Layer 1 program with zero chunks — it must NOT throw.
		ConnectModeParser p(";;;;;;;");
		REQUIRE(p.AllRemaining().empty());
	}
}

TEST_CASE("ConnectModeParser: plain SQL is captured as RAW chunks", "[connect_mode]") {
	SECTION("single statement") {
		ConnectModeParser p("SELECT 1");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::RAW);
		REQUIRE(chunks[0].text == "SELECT 1");
	}

	SECTION("two statements split on ;") {
		ConnectModeParser p("SELECT 1; SELECT 2");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 2);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::RAW);
		// chunk.text round-trips the trailing `;` so downstream consumers (e.g. QueryLog) see the
		// user-typed statement faithfully.
		REQUIRE(chunks[0].text == "SELECT 1;");
		REQUIRE(chunks[1].type == ConnectModeChunk::Type::RAW);
		REQUIRE(chunks[1].text == "SELECT 2");
	}

	SECTION("trailing semicolon preserved on single statement") {
		ConnectModeParser p("SELECT 1;");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].text == "SELECT 1;");
	}

	SECTION("leading semicolons are tolerated") {
		ConnectModeParser p(";SELECT 1");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].text == "SELECT 1");
	}

	SECTION("multiple consecutive separator semicolons") {
		ConnectModeParser p("SELECT 42;;;SELECT 10;");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 2);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::RAW);
		REQUIRE(chunks[1].type == ConnectModeChunk::Type::RAW);
	}

	SECTION("multiple trailing semicolons after last statement") {
		ConnectModeParser p("SELECT 1;;;;;");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::RAW);
	}

	SECTION("PIVOT statement captured opaquely (no decomposition)") {
		ConnectModeParser p("PIVOT t ON x USING SUM(y)");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::RAW);
		REQUIRE(chunks[0].text == "PIVOT t ON x USING SUM(y)");
	}

	SECTION("embedded semicolon inside string literal does not split") {
		ConnectModeParser p("SELECT 'a;b;c'");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].text == "SELECT 'a;b;c'");
	}
}

TEST_CASE("ConnectModeParser: well-formed CONNECT/DISCONNECT classified as CONTROL", "[connect_mode]") {
	// (bare `CONNECT` is now classified as FORBIDDEN with MISSING_CONNECT_TARGET — see the
	// "malformed CONNECT" test case below.)


	SECTION("CONNECT name") {
		ConnectModeParser p("CONNECT pg");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::CONTROL);
		REQUIRE(chunks[0].text == "CONNECT pg");
	}

	SECTION("CONNECT LOCAL") {
		ConnectModeParser p("CONNECT LOCAL");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::CONTROL);
	}

	SECTION("CONNECT 'uri'") {
		ConnectModeParser p("CONNECT 'quack:localhost'");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::CONTROL);
	}

	SECTION("DISCONNECT") {
		ConnectModeParser p("DISCONNECT");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::CONTROL);
	}
}

TEST_CASE("ConnectModeParser: CONNECT name EXECUTE payload classified as EXECUTE", "[connect_mode]") {
	SECTION("simple SELECT payload") {
		ConnectModeParser p("CONNECT pg EXECUTE SELECT 1");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::EXECUTE);
		REQUIRE(chunks[0].target == "pg");
		REQUIRE(chunks[0].payload == "SELECT 1");
	}

	SECTION("PIVOT payload (the original chokepoint use case)") {
		ConnectModeParser p("CONNECT pg EXECUTE PIVOT t ON x USING SUM(y)");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::EXECUTE);
		REQUIRE(chunks[0].target == "pg");
		REQUIRE(chunks[0].payload == "PIVOT t ON x USING SUM(y)");
	}
}

TEST_CASE("ConnectModeParser: malformed CONNECT classified as FORBIDDEN", "[connect_mode]") {
	SECTION("extra tokens after CONNECT identifier target") {
		ConnectModeParser p("CONNECT pg foo bar");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::FORBIDDEN);
		REQUIRE(chunks[0].forbidden_reason ==
		        ConnectModeChunk::ForbiddenReason::EXTRA_TOKENS_AFTER_CONNECT_TARGET);
	}

	SECTION("extra tokens after CONNECT LOCAL") {
		ConnectModeParser p("CONNECT LOCAL extra");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::FORBIDDEN);
		REQUIRE(chunks[0].forbidden_reason ==
		        ConnectModeChunk::ForbiddenReason::EXTRA_TOKENS_AFTER_CONNECT_TARGET);
	}

	SECTION("extra tokens after CONNECT 'string'") {
		ConnectModeParser p("CONNECT 'quack:localhost' extra");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::FORBIDDEN);
		REQUIRE(chunks[0].forbidden_reason ==
		        ConnectModeChunk::ForbiddenReason::EXTRA_TOKENS_AFTER_CONNECT_TARGET);
	}

	SECTION("CONNECT with non-identifier target (numeric)") {
		ConnectModeParser p("CONNECT 42");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::FORBIDDEN);
		REQUIRE(chunks[0].forbidden_reason == ConnectModeChunk::ForbiddenReason::INVALID_CONNECT_TARGET);
	}

	SECTION("CONNECT EXECUTE without target") {
		// Caught by the specific 'CONNECT' 'EXECUTE' Raw rule (ForbiddenConnectExecute), which is
		// ordered before ForbiddenConnect1 so the EXECUTE keyword is not mistakenly absorbed as an
		// identifier target.
		ConnectModeParser p("CONNECT EXECUTE SELECT 1");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::FORBIDDEN);
		REQUIRE(chunks[0].forbidden_reason ==
		        ConnectModeChunk::ForbiddenReason::CONNECT_EXECUTE_MISSING_TARGET);
	}

	SECTION("bare CONNECT — missing target") {
		ConnectModeParser p("CONNECT");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::FORBIDDEN);
		REQUIRE(chunks[0].forbidden_reason == ConnectModeChunk::ForbiddenReason::MISSING_CONNECT_TARGET);
	}

	SECTION("DISCONNECT with extra tokens") {
		ConnectModeParser p("DISCONNECT xyz");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::FORBIDDEN);
		REQUIRE(chunks[0].forbidden_reason ==
		        ConnectModeChunk::ForbiddenReason::EXTRA_TOKENS_AFTER_DISCONNECT);
	}
}

TEST_CASE("ConnectModeParser: EXECUTE payload is opaque (keywords allowed)", "[connect_mode]") {
	SECTION("payload starting with CONNECT is just text") {
		ConnectModeParser p("CONNECT pg EXECUTE CONNECT inner");
		auto chunks = p.AllRemaining();
		REQUIRE(chunks.size() == 1);
		REQUIRE(chunks[0].type == ConnectModeChunk::Type::EXECUTE);
		REQUIRE(chunks[0].target == "pg");
		// payload is the raw text after EXECUTE; "CONNECT inner" is just forwarded as-is
	}
}

TEST_CASE("ConnectModeParser: multi-statement mixed chunks", "[connect_mode]") {
	ConnectModeParser p("SELECT 1; CONNECT pg; PIVOT t ON x USING SUM(y); DISCONNECT");
	auto chunks = p.AllRemaining();
	REQUIRE(chunks.size() == 4);
	REQUIRE(chunks[0].type == ConnectModeChunk::Type::RAW);
	REQUIRE(chunks[1].type == ConnectModeChunk::Type::CONTROL);
	REQUIRE(chunks[2].type == ConnectModeChunk::Type::RAW);
	REQUIRE(chunks[2].text == "PIVOT t ON x USING SUM(y);");
	REQUIRE(chunks[3].type == ConnectModeChunk::Type::CONTROL);
}

TEST_CASE("ConnectModeParser: TryGetNext is iterator-style", "[connect_mode]") {
	ConnectModeParser p("CONNECT pg; SELECT 1");
	ConnectModeChunk chunk;

	REQUIRE(p.TryGetNext(chunk));
	REQUIRE(chunk.type == ConnectModeChunk::Type::CONTROL);

	REQUIRE(p.TryGetNext(chunk));
	REQUIRE(chunk.type == ConnectModeChunk::Type::RAW);
	REQUIRE(chunk.text == "SELECT 1");

	REQUIRE_FALSE(p.TryGetNext(chunk));
}
