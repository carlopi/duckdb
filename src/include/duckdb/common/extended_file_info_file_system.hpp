//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/extended_file_info_file_system.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/file_system.hpp"
#include "duckdb/common/types/value.hpp"

namespace duckdb {

//! Handles paths that carry their open options inline: duckdb_file_info://{key=value;key=value}path
//! Options are written as text and read back through ExtendedOpenFileInfo::TryGetOption, which casts on request
//! It performs no I/O of its own: it rewrites such a path into the file it stands for, which is then dispatched
//! as usual. Globbing a packed path yields just that file, since it names a file the caller knows the identity of
class ExtendedFileInfoFileSystem : public FileSystem {
public:
	static constexpr const char *NAME = "ExtendedFileInfoFileSystem";
	static constexpr const char *ENCODED_PATH_PREFIX = "duckdb_file_info://";

public:
	std::string GetName() const override;
	bool CanHandleFile(const string &fpath) override;
	bool IsRewritingFileSystem() const override;
	OpenFileInfo RewriteFile(const OpenFileInfo &file) override;

public:
	//! Whether a path carries its open options inline
	DUCKDB_API static bool IsEncodedPath(const string &path);
	//! Encode a file together with its open options into a single path that any path-taking API accepts.
	//! A file without extended info encodes to its plain path
	DUCKDB_API static string EncodePath(const OpenFileInfo &file);
	//! Decode a path produced by EncodePath. Returns false when the path is a plain path, throws when the path
	//! starts with the prefix but is malformed
	DUCKDB_API static bool TryDecodePath(const string &path, OpenFileInfo &result);
	//! Decode a path if it is encoded, otherwise return it as-is
	DUCKDB_API static OpenFileInfo Decode(const string &path);
	//! Decode a file whose path may be encoded. Options explicitly set on the file win over encoded ones
	DUCKDB_API static OpenFileInfo Decode(const OpenFileInfo &file);
	//! A file as a STRUCT value holding the path in the "filename" field and every option as another field
	DUCKDB_API static Value ToValue(const OpenFileInfo &file);

protected:
	unique_ptr<MultiFileList> GlobFilesExtended(const string &path, const FileGlobInput &input,
	                                            optional_ptr<FileOpener> opener) override;
	bool SupportsGlobExtended() const override {
		return true;
	}
};

} // namespace duckdb
