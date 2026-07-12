//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/redirect_info.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/constants.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/pair.hpp"

namespace duckdb {
class WriteStream;
class ReadStream;

//! A single stored ATTACH option carried by a redirect record.
struct RedirectOption {
	string key;
	string value;
};

//! An optional record, appended to the MainHeader, that turns a DuckDB file into a pointer: on ATTACH the
//! pointer file is not opened, but the ATTACH is reinterpreted to open `target` (as storage type `type`).
struct RedirectInfo {
	//! The record format version, independently versioned from the storage format so the layout can grow.
	static constexpr uint8_t CURRENT_VERSION = 1;
	//! Cap on the serialized record size so it always fits inside the checksummed 4KiB header block.
	static constexpr idx_t MAX_RECORD_SIZE = 3072;

	//! Whether a redirect record is present.
	bool is_redirect = false;
	//! The record format version.
	uint8_t version = CURRENT_VERSION;
	//! The storage type of the target (e.g. duckdb / iceberg / postgres / sqlite). Empty means duckdb.
	string type;
	//! The path / URI / connection target of the database this pointer redirects to.
	string target;
	//! Stored ATTACH options merged into the reinterpreted ATTACH (user-supplied options win).
	vector<RedirectOption> options;

	bool IsRedirect() const {
		return is_redirect;
	}

	//! Serialize the record body (everything after the MainHeader flags).
	void Serialize(WriteStream &ser) const;
	//! Deserialize a record body written by Serialize.
	static RedirectInfo Deserialize(ReadStream &source);
};

} // namespace duckdb
