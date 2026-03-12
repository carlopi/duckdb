//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_column_data_collection.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/types/column/column_data_collection.hpp"

namespace duckdb_shell {

using duckdb::idx_t;

//! ShellColumnDataCollection wraps a duckdb::ColumnDataCollection reference.
class ShellColumnDataCollection {
	duckdb::ColumnDataCollection &collection;

public:
	explicit ShellColumnDataCollection(duckdb::ColumnDataCollection &collection);

	idx_t Count() const;
	idx_t ColumnCount() const;
	duckdb::ColumnDataRowCollection GetRows() const;

	//! Engine access — needed for BoxRenderer::Prepare and ColumnDataRef
	duckdb::ColumnDataCollection &GetCollection();
};

} // namespace duckdb_shell
