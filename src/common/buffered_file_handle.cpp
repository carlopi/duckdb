
#include "duckdb/common/buffered_file_handle.hpp"

#include <iostream>
namespace duckdb {

BufferedFileHandle::BufferedFileHandle(FileHandle &inner_handle, size_t start, size_t end, Allocator &allocator) : inner_handle(inner_handle), start(start), end(end), allocator(allocator) {
	if (start > end) {
		throw InvalidInputException("Range in BufferedFileHandle constructor wrongly set");
	}
	buffered = allocator.Allocate(end - start);

	inner_handle.Read(buffered.get(),  end - start, start);
}

BufferedFileHandle::~BufferedFileHandle() {
	// buffered is RAII managed
}

void BufferedFileHandle::Read(void *buffer, idx_t nr_bytes, idx_t location) {
	std::cout << "BufferedFileHandle::Read " << nr_bytes << " from  " << location << "\n";
	data_ptr_t ptr = static_cast<data_ptr_t>(buffer);
/*
	idx_t original_location = location;
	if (location < start) {
		inner_handle.Read(ptr, start-location, location);
		ptr += start-location;
		location = start;
	}
	if (location < end) {
*/
		std::cout << (int)buffered.get()[8] << (int)buffered.get()[9] << (int)buffered.get()[10] <<(int) buffered.get()[11] << "\n";
		memcpy(ptr, buffered.get() + location, nr_bytes);
		std::cout << (int)ptr[0] << (int)ptr[1] << (int)ptr[2] <<(int) ptr[3] << "\n";
		ptr += nr_bytes;
		// FIXME, copy from buffer
/*
	}
	if (original_location + nr_bytes > end) {
		inner_handle.Read(ptr, end, original_location+nr_bytes - end);
	}
*/
}

idx_t BufferedFileHandle::GetFileSize() {
	return inner_handle.GetFileSize();
}

string BufferedFileHandle::GetPath() {
	return inner_handle.GetPath();
}

}; // namespace
