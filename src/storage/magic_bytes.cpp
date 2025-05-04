#include "duckdb/storage/magic_bytes.hpp"
#include "duckdb/common/local_file_system.hpp"
#include "duckdb/storage/storage_info.hpp"

namespace duckdb {

DataFileType MagicBytes::CheckMagicBytes(FileSystem &fs, const string &path, unique_ptr<FileHandle> &file_handle) {
	if (path.empty() || path == IN_MEMORY_PATH) {
		return DataFileType::DUCKDB_FILE;
	}
	if (!file_handle) {
		auto handle = fs.OpenFile(path, FileFlags::FILE_FLAGS_READ | FileFlags::FILE_FLAGS_NULL_IF_NOT_EXISTS |
		                                    FileFlags::FILE_FLAGS_PARALLEL_ACCESS | FileLockType::READ_LOCK);
		if (!handle) {
			return DataFileType::FILE_DOES_NOT_EXIST;
		}
		file_handle = std::move(handle);
	}

	constexpr const idx_t MAGIC_BYTES_READ_SIZE = 16;
	char buffer[MAGIC_BYTES_READ_SIZE] = {};

	file_handle->Read(buffer, MAGIC_BYTES_READ_SIZE);
	if (memcmp(buffer, "SQLite format 3\0", 16) == 0) {
		return DataFileType::SQLITE_FILE;
	}
	if (memcmp(buffer, "PAR1", 4) == 0) {
		return DataFileType::PARQUET_FILE;
	}
	if (memcmp(buffer + MainHeader::MAGIC_BYTE_OFFSET, MainHeader::MAGIC_BYTES, MainHeader::MAGIC_BYTE_SIZE) == 0) {
		return DataFileType::DUCKDB_FILE;
	}
	return DataFileType::FILE_DOES_NOT_EXIST;
}

} // namespace duckdb
