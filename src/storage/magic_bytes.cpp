#include "duckdb/storage/magic_bytes.hpp"
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

//! Parse a redirect record from the already-prefetched header bytes. A malformed or partial header simply
//! yields no redirect - the normal storage open path surfaces genuine corruption. Encrypted databases are
//! skipped: their redirect record would be ciphertext, and the key is not available during detection.
static void ParseRedirect(char *buffer, idx_t buffer_size, RedirectInfo &out_redirect) {
	if (buffer_size <= MainHeader::MAGIC_BYTE_OFFSET) {
		return;
	}
	try {
		MemoryStream stream(reinterpret_cast<data_ptr_t>(buffer) + MainHeader::MAGIC_BYTE_OFFSET,
		                    buffer_size - MainHeader::MAGIC_BYTE_OFFSET);
		// Skip the storage-version gate so a poisoned pointer file is still detected as a redirect.
		auto header = MainHeader::Read(stream, false);
		if (!header.IsEncrypted() && header.IsRedirect()) {
			out_redirect = std::move(header.redirect);
		}
	} catch (const std::exception &) {
		// Not a readable redirect record - leave out_redirect empty.
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
			ParseRedirect(buffer.get(), to_read, *out_redirect);
		}
	}
	return file_type;
}

} // namespace duckdb
