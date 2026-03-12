#include "shell_column_data_collection.hpp"

namespace duckdb_shell {

ShellColumnDataCollection::ShellColumnDataCollection(duckdb::ColumnDataCollection &collection)
    : collection(collection) {
}

idx_t ShellColumnDataCollection::Count() const {
	return collection.Count();
}

idx_t ShellColumnDataCollection::ColumnCount() const {
	return collection.ColumnCount();
}

duckdb::ColumnDataRowCollection ShellColumnDataCollection::GetRows() const {
	return collection.GetRows();
}

duckdb::ColumnDataCollection &ShellColumnDataCollection::GetCollection() {
	return collection;
}

} // namespace duckdb_shell
