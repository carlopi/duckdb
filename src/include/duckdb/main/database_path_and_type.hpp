//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/main/database_path_and_type.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <string>
#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/main/config.hpp"

namespace duckdb {

class QueryContext;
struct PrefetchedFileData;
struct RedirectInfo;

struct DBPathAndType {
	//! Parse database extension type and rest of path from combined form (type:path)
	static void ExtractExtensionPrefix(string &path, string &db_type);
	//! Check the magic bytes of a file and set the database type. For a DuckDB file, the prefetched header is
	//! returned through `out_prefetch` when set, and a redirect record (if any) through `out_redirect`.
	static void CheckMagicBytes(QueryContext context, FileSystem &fs, string &path, string &db_type,
	                            optional_ptr<PrefetchedFileData> out_prefetch = nullptr,
	                            optional_ptr<RedirectInfo> out_redirect = nullptr);

	//! Run ExtractExtensionPrefix followed by CheckMagicBytes. Same `out_prefetch`/`out_redirect` semantics.
	static void ResolveDatabaseType(FileSystem &fs, string &path, string &db_type,
	                                optional_ptr<PrefetchedFileData> out_prefetch = nullptr,
	                                optional_ptr<RedirectInfo> out_redirect = nullptr);
};
} // namespace duckdb
