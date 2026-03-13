// Wire-mode stub for ColumnDataAllocator — only supports IN_MEMORY_ALLOCATOR.
// BufferManager paths throw since wire mode doesn't have a local storage engine.

#include "duckdb/common/types/column/column_data_allocator.hpp"
#include "duckdb/common/types/column/column_data_collection_segment.hpp"

namespace duckdb {

static const char *WIRE_MODE_BM_ERROR = "ColumnDataAllocator: BufferManager not available in wire mode";

ColumnDataAllocator::ColumnDataAllocator(Allocator &allocator) : type(ColumnDataAllocatorType::IN_MEMORY_ALLOCATOR) {
	alloc.allocator = &allocator;
}

ColumnDataAllocator::ColumnDataAllocator(BufferManager &, ColumnDataCollectionLifetime) : type(ColumnDataAllocatorType::BUFFER_MANAGER_ALLOCATOR) {
	throw InternalException(WIRE_MODE_BM_ERROR);
}

ColumnDataAllocator::ColumnDataAllocator(ClientContext &, ColumnDataAllocatorType allocator_type, ColumnDataCollectionLifetime)
    : type(allocator_type) {
	throw InternalException(WIRE_MODE_BM_ERROR);
}

ColumnDataAllocator::ColumnDataAllocator(ColumnDataAllocator &other) {
	type = other.GetType();
	if (type == ColumnDataAllocatorType::IN_MEMORY_ALLOCATOR) {
		alloc.allocator = other.alloc.allocator;
	} else {
		throw InternalException(WIRE_MODE_BM_ERROR);
	}
}

ColumnDataAllocator::~ColumnDataAllocator() {
}

void ColumnDataAllocator::AllocateData(idx_t size, uint32_t &block_id, uint32_t &offset,
                                       ChunkManagementState *chunk_state) {
	if (type == ColumnDataAllocatorType::IN_MEMORY_ALLOCATOR) {
		D_ASSERT(blocks.size() == allocated_data.size());
		if (blocks.empty() || blocks.back().Capacity() < size) {
			auto allocation_amount = MaxValue<idx_t>(NextPowerOfTwo(size), 4096);
			if (!blocks.empty()) {
				idx_t last_capacity = blocks.back().capacity;
				auto next_capacity = MinValue<idx_t>(last_capacity * 2, last_capacity + Storage::DEFAULT_BLOCK_SIZE);
				allocation_amount = MaxValue<idx_t>(next_capacity, allocation_amount);
			}
			BlockMetaData data;
			data.size = 0;
			data.capacity = NumericCast<uint32_t>(allocation_amount);
			blocks.push_back(std::move(data));
			allocated_size += allocation_amount;
			auto allocated = alloc.allocator->Allocate(data.capacity);
			allocated_data.push_back(std::move(allocated));
		}
		auto &block = blocks.back();
		D_ASSERT(size <= block.capacity - block.size);
		// construct pointer from block_id and offset
		auto pointer = allocated_data.back().get() + block.size;
		auto pointer_value = uintptr_t(pointer);
		if (sizeof(uintptr_t) == sizeof(uint32_t)) {
			block_id = uint32_t(pointer_value);
			offset = 0;
		} else if (sizeof(uintptr_t) == sizeof(uint64_t)) {
			block_id = uint32_t(pointer_value & 0xFFFFFFFF);
			offset = uint32_t(pointer_value >> 32);
		} else {
			throw InternalException("ColumnDataCollection: Architecture not supported!?");
		}
		block.size += size;
	} else {
		throw InternalException(WIRE_MODE_BM_ERROR);
	}
}

data_ptr_t ColumnDataAllocator::GetDataPointer(ChunkManagementState &, uint32_t block_id, uint32_t offset) {
	if (type == ColumnDataAllocatorType::IN_MEMORY_ALLOCATOR) {
		if (sizeof(uintptr_t) == sizeof(uint32_t)) {
			uintptr_t pointer_value = uintptr_t(block_id);
			return (data_ptr_t)pointer_value; // NOLINT
		} else if (sizeof(uintptr_t) == sizeof(uint64_t)) {
			uintptr_t pointer_value = (uintptr_t(offset) << 32) | uintptr_t(block_id);
			return (data_ptr_t)pointer_value; // NOLINT
		} else {
			throw InternalException("ColumnDataCollection: Architecture not supported!?");
		}
	}
	throw InternalException(WIRE_MODE_BM_ERROR);
}

void ColumnDataAllocator::UnswizzlePointers(ChunkManagementState &, Vector &, SwizzleMetaData &,
                                            const VectorMetaData &, const idx_t &, const bool &) {
	throw InternalException(WIRE_MODE_BM_ERROR);
}

void ColumnDataAllocator::InitializeChunkState(ChunkManagementState &, ChunkMetaData &) {
	// nothing to pin for in-memory allocator
	if (type != ColumnDataAllocatorType::IN_MEMORY_ALLOCATOR) {
		throw InternalException(WIRE_MODE_BM_ERROR);
	}
}

Allocator &ColumnDataAllocator::GetAllocator() {
	if (type == ColumnDataAllocatorType::IN_MEMORY_ALLOCATOR) {
		return *alloc.allocator;
	}
	throw InternalException(WIRE_MODE_BM_ERROR);
}

BufferManager &ColumnDataAllocator::GetBufferManager() {
	throw InternalException(WIRE_MODE_BM_ERROR);
}

shared_ptr<DatabaseInstance> ColumnDataAllocator::GetDatabase() const {
	return nullptr;
}

shared_ptr<BlockHandle> BlockMetaData::GetHandle() const {
	throw InternalException(WIRE_MODE_BM_ERROR);
}

void BlockMetaData::SetHandle(ManagedResultSet &, shared_ptr<BlockHandle>) {
	throw InternalException(WIRE_MODE_BM_ERROR);
}

uint32_t BlockMetaData::Capacity() {
	D_ASSERT(size <= capacity);
	return capacity - size;
}

} // namespace duckdb
