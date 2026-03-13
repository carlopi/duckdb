#include "duckdb/common/box_renderer_context.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"

namespace duckdb {

// ===== BoxRendererContext (base) =====

void BoxRendererContext::CastToVarchar(Vector &source, Vector &result, idx_t count, bool as_json) {
	DataChunk source_chunk;
	source_chunk.InitializeEmpty({source.GetType()});
	source_chunk.data[0].Reference(source);
	source_chunk.SetCardinality(count);

	DataChunk result_chunk;
	result_chunk.InitializeEmpty({LogicalType::VARCHAR});
	result_chunk.data[0].Reference(result);
	result_chunk.SetCardinality(count);

	CastToVarchar(source_chunk, result_chunk, count, as_json);
}

// ===== ClientBoxRendererContext =====

ClientBoxRendererContext::ClientBoxRendererContext(ClientContext &context) : context(context) {
}

bool ClientBoxRendererContext::IsInterrupted() const {
	return context.IsInterrupted();
}

void ClientBoxRendererContext::CastToVarchar(DataChunk &source, DataChunk &result, idx_t count, bool as_json) {
	for (idx_t c = 0; c < source.ColumnCount(); c++) {
		VectorOperations::TryCast(context, source.data[c], result.data[c], count, nullptr, false);
	}
}

Allocator &ClientBoxRendererContext::GetAllocator() {
	return Allocator::Get(context);
}

} // namespace duckdb
