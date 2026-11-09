//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/external_resource_family.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/sql_identifier.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

//! The family of an external resource type is the leading segment of its name: 'aws' for
//! 'aws:s3tables:table-bucket'. The statements name it in place of EXTERNAL, `CREATE AWS RESOURCE 's3tables:...'`,
//! `SHOW ALL AWS RESOURCES`, and EXTERNAL is the root: every type, spelled in full. A family parses as an
//! identifier, so an extension plugs one in by registering types under it; an unknown family fails at bind.
//! Like an identifier it is case-insensitive, so it is folded to lower case, the case type names use.
struct ExternalResourceFamily {
	static constexpr const char *ROOT = "external";

	//! EXTERNAL, or no family at all
	static bool IsRoot(const string &family) {
		return family.empty() || StringUtil::CIEquals(family, ROOT);
	}
	//! The full type name: 'aws' + 's3tables' is 'aws:s3tables'; under the root the type is already spelled in full
	static string QualifyType(const string &family, const string &type) {
		return IsRoot(family) ? type : StringUtil::Lower(family) + ":" + type;
	}
	//! Whether `type` is `family` itself or below it. The root contains every type.
	static bool Contains(const string &family, const string &type) {
		if (IsRoot(family)) {
			return true;
		}
		auto folded = StringUtil::Lower(family);
		return type == folded || StringUtil::StartsWith(type, folded + ":");
	}
	//! The family as a statement spells it: EXTERNAL, or the identifier
	static string Keyword(const string &family) {
		return IsRoot(family) ? string("EXTERNAL") : SQLIdentifier::ToString(family);
	}
};

} // namespace duckdb
