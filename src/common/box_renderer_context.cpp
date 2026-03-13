#include "duckdb/common/box_renderer_context.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/types/data_chunk.hpp"

namespace duckdb {

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

} // namespace duckdb
