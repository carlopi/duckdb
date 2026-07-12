//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/magic_bytes.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/optional_ptr.hpp"

namespace duckdb {
class FileSystem;
class QueryContext;
struct PrefetchedFileData;
struct RedirectInfo;

enum class DataFileType : uint8_t {
	FILE_DOES_NOT_EXIST, // file does not exist
	DUCKDB_FILE,         // duckdb database file
	SQLITE_FILE,         // sqlite database file
	PARQUET_FILE,        // parquet file
	UNKNOWN_FILE         // unknown file type
};

class MagicBytes {
public:
	//! Detect the file type at `path` from its magic bytes. For a DuckDB file, a prefetched header prefix is
	//! returned through `out_prefetch` (when set) so a later storage open reuses it instead of re-reading.
	//! When `out_redirect` is set and the DuckDB file carries a redirect record, it is parsed from the
	//! prefetched bytes into `out_redirect` (no extra I/O).
	static DataFileType CheckMagicBytes(QueryContext context, FileSystem &fs, const string &path,
	                                    optional_ptr<PrefetchedFileData> out_prefetch = nullptr,
	                                    optional_ptr<RedirectInfo> out_redirect = nullptr);
};

} // namespace duckdb
