#include "duckdb/storage/magic_bytes.hpp"
#include "duckdb/common/checksum.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/prefetched_file_data.hpp"
#include "duckdb/common/serializer/memory_stream.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/storage/storage_info.hpp"
#include "duckdb/storage/redirect_info.hpp"

#include <cstring>

namespace duckdb {

static constexpr idx_t MAGIC_BYTES_READ_SIZE = 16;
//! MainHeader + 2 DatabaseHeaders; prefetched so a later storage open serves the header reads from memory.
static constexpr idx_t HEADER_PREFETCH_SIZE = 3 * Storage::FILE_HEADER_SIZE;

static DataFileType ClassifyMagicBytes(const char *buffer) {
	if (memcmp(buffer, "SQLite format 3\0", 16) == 0) {
		return DataFileType::SQLITE_FILE;
	}
	if (memcmp(buffer, "PAR1", 4) == 0) {
		return DataFileType::PARQUET_FILE;
	}
	if (memcmp(buffer + MainHeader::MAGIC_BYTE_OFFSET, MainHeader::MAGIC_BYTES, MainHeader::MAGIC_BYTE_SIZE) == 0) {
		return DataFileType::DUCKDB_FILE;
	}
	return DataFileType::UNKNOWN_FILE;
}

//! Verify a header block's self-checksum: the first 8 bytes hold the checksum over the remaining data region.
//! The main header and (unencrypted) database headers are plaintext and use this same layout.
static bool VerifyHeaderBlockChecksum(const_data_ptr_t block_start) {
	auto stored = Load<uint64_t>(block_start);
	auto computed =
	    Checksum(block_start + MainHeader::MAGIC_BYTE_OFFSET, Storage::FILE_HEADER_SIZE - MainHeader::MAGIC_BYTE_OFFSET);
	return stored == computed;
}

//! Parse a redirect record from the already-prefetched header bytes. The MainHeader (block 0) carries the redirect
//! flag; the record itself lives in the DatabaseHeaders (blocks 1/2), the active one chosen by iteration count -
//! mirroring how a re-point atomically swaps the active slot. Every block is checksum-verified before it is trusted,
//! so a single corrupted byte cannot silently attach a wrong target. A malformed or partial header yields no
//! redirect (the normal open path surfaces genuine corruption); a redirect-flagged file with no readable record is
//! reported as corrupt. Encrypted databases are skipped: their record would be ciphertext, and the key is not
//! available during detection.
static void ParseRedirect(const string &path, char *buffer, idx_t buffer_size, RedirectInfo &out_redirect) {
	// The record lives in the DatabaseHeaders, so the full 3-block header region must have been read.
	if (buffer_size < 3 * Storage::FILE_HEADER_SIZE) {
		return;
	}
	auto data = reinterpret_cast<data_ptr_t>(buffer);

	// Only trust the redirect flag if the MainHeader block itself is intact; otherwise let the normal open path
	// report the corruption.
	if (!VerifyHeaderBlockChecksum(data)) {
		return;
	}
	MainHeader main_header {};
	try {
		MemoryStream main_stream(data + MainHeader::MAGIC_BYTE_OFFSET,
		                         Storage::FILE_HEADER_SIZE - MainHeader::MAGIC_BYTE_OFFSET);
		// Skip the storage-version gate so a poisoned pointer file is still detected as a redirect.
		main_header = MainHeader::Read(main_stream, false);
	} catch (const std::exception &) {
		return;
	}
	if (main_header.IsEncrypted() || !main_header.IsRedirect()) {
		return;
	}

	// Pick the valid slot with the highest iteration - a torn re-point leaves the older slot intact as a fallback.
	bool found = false;
	uint64_t best_iteration = 0;
	for (idx_t slot = 1; slot <= 2; slot++) {
		auto block_start = data + slot * Storage::FILE_HEADER_SIZE;
		if (!VerifyHeaderBlockChecksum(block_start)) {
			continue;
		}
		try {
			MemoryStream db_stream(block_start + MainHeader::MAGIC_BYTE_OFFSET,
			                       Storage::FILE_HEADER_SIZE - MainHeader::MAGIC_BYTE_OFFSET);
			auto db_header = DatabaseHeader::Read(main_header, db_stream);
			if (db_header.redirect.is_redirect && (!found || db_header.iteration >= best_iteration)) {
				out_redirect = std::move(db_header.redirect);
				best_iteration = db_header.iteration;
				found = true;
			}
		} catch (const std::exception &) {
			// A checksum-valid slot with an unreadable record - skip it; the other slot may still be usable.
		}
	}
	if (!found) {
		throw IOException(
		    "Corrupt redirect pointer file \"%s\": the redirect flag is set but no valid redirect record was found",
		    path);
	}
}

DataFileType MagicBytes::CheckMagicBytes(QueryContext context, FileSystem &fs, const string &path,
                                         optional_ptr<PrefetchedFileData> out_prefetch,
                                         optional_ptr<RedirectInfo> out_redirect) {
	if (path.empty() || path == IN_MEMORY_PATH) {
		return DataFileType::DUCKDB_FILE;
	}

	// The handle only reads the header and is closed on return; the database handle is opened later. On prefetch
	// we read the whole header region at once so the later open can serve the header reads from these bytes.
	auto handle = fs.OpenFile(path, FileFlags::FILE_FLAGS_READ | FileFlags::FILE_FLAGS_NULL_IF_NOT_EXISTS);
	if (!handle) {
		return DataFileType::FILE_DOES_NOT_EXIST;
	}

	// Zero-initialized so a short read at EOF still yields a well-defined (non-matching) classification.
	// Reading the full header region is needed both to prefetch and to parse a redirect record.
	const bool read_full_header = out_prefetch || out_redirect;
	const idx_t read_size = read_full_header ? HEADER_PREFETCH_SIZE : MAGIC_BYTES_READ_SIZE;
	auto buffer = make_unsafe_uniq_array<char>(read_size);
	memset(buffer.get(), 0, read_size);
	const idx_t to_read = MinValue<idx_t>(read_size, handle->GetFileSize());
	if (to_read > 0) {
		handle->Read(context, buffer.get(), to_read, 0);
	}

	auto file_type = ClassifyMagicBytes(buffer.get());

	if (file_type == DataFileType::DUCKDB_FILE) {
		// Hand the prefetched header bytes to the caller. Only the `to_read` valid bytes are kept.
		if (out_prefetch) {
			out_prefetch->header = make_shared_ptr<const string>(buffer.get(), to_read);
		}
		// Parse a redirect record (if any) from the same bytes - no extra I/O.
		if (out_redirect) {
			ParseRedirect(path, buffer.get(), to_read, *out_redirect);
		}
	}
	return file_type;
}

} // namespace duckdb
