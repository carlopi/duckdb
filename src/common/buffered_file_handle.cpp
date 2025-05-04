
#include "duckdb/common/buffered_file_handle.hpp"

#include <iostream>
namespace duckdb {

BufferedFileHandle::BufferedFileHandle(FileHandle &inner_handle, size_t start, size_t end, Allocator &allocator)
    : inner_handle(inner_handle), start(start), end(end), allocator(allocator) {
	if (start > end) {
		throw InvalidInputException("Range in BufferedFileHandle constructor wrongly set");
	}
	buffered = allocator.Allocate(end - start);

	inner_handle.Read(buffered.get(), end - start, start);
}

BufferedFileHandle::~BufferedFileHandle() {
	// buffered is RAII managed
}

void BufferedFileHandle::Read(void *buffer, idx_t nr_bytes, idx_t location) {
	std::cout << "BufferedFileHandle::Read " << nr_bytes << " from  " << location << "\n";
	data_ptr_t ptr = static_cast<data_ptr_t>(buffer);
	idx_t current_location = location;

	if (current_location < start) {
		inner_handle.Read(ptr, start - current_location, current_location);
		ptr += start - current_location;
		current_location = start;
	}
	if (current_location < end) {
		memcpy(ptr, buffered.get() + current_location, nr_bytes);
		ptr += nr_bytes;
		current_location += nr_bytes;
	}
	if (location + nr_bytes > end) {
		inner_handle.Read(ptr, end - current_location, current_location);
	}
}

idx_t BufferedFileHandle::GetFileSize() {
	return inner_handle.GetFileSize();
}

string BufferedFileHandle::GetPath() {
	return inner_handle.GetPath();
}

}; // namespace duckdb
