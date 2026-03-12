#include "shell_query_result.hpp"

namespace duckdb_shell {

// ===== ShellQueryResult =====

ShellQueryResult::ShellQueryResult(duckdb::unique_ptr<duckdb::QueryResult> result) : result(std::move(result)) {
}

ShellQueryResult::~ShellQueryResult() {
}

bool ShellQueryResult::HasError() const {
	return result->HasError();
}

const duckdb::string &ShellQueryResult::GetError() const {
	return result->GetError();
}

duckdb::StatementReturnType ShellQueryResult::GetReturnType() const {
	return result->properties.return_type;
}

duckdb::QueryResultType ShellQueryResult::GetResultType() const {
	return result->type;
}

idx_t ShellQueryResult::ColumnCount() const {
	return result->ColumnCount();
}

const duckdb::vector<duckdb::string> &ShellQueryResult::Names() const {
	return result->names;
}

const duckdb::vector<duckdb::LogicalType> &ShellQueryResult::Types() const {
	return result->types;
}

duckdb::unique_ptr<duckdb::DataChunk> ShellQueryResult::Fetch() {
	return result->Fetch();
}

duckdb::QueryResult &ShellQueryResult::GetResult() {
	return *result;
}

duckdb::unique_ptr<duckdb::MaterializedQueryResult> ShellQueryResult::TakeMaterialized() {
	return duckdb::unique_ptr_cast<duckdb::QueryResult, duckdb::MaterializedQueryResult>(std::move(result));
}

// ===== ShellMaterializedQueryResult =====

ShellMaterializedQueryResult::ShellMaterializedQueryResult(duckdb::unique_ptr<duckdb::MaterializedQueryResult> result)
    : result(std::move(result)) {
}

ShellMaterializedQueryResult::~ShellMaterializedQueryResult() {
}

bool ShellMaterializedQueryResult::HasError() const {
	return result->HasError();
}

const duckdb::string &ShellMaterializedQueryResult::GetError() const {
	return result->GetError();
}

duckdb::StatementReturnType ShellMaterializedQueryResult::GetReturnType() const {
	return result->properties.return_type;
}

const duckdb::vector<duckdb::string> &ShellMaterializedQueryResult::Names() const {
	return result->names;
}

const duckdb::vector<duckdb::LogicalType> &ShellMaterializedQueryResult::Types() const {
	return result->types;
}

idx_t ShellMaterializedQueryResult::ColumnCount() const {
	return result->ColumnCount();
}

idx_t ShellMaterializedQueryResult::RowCount() const {
	return result->RowCount();
}

ShellColumnDataCollection ShellMaterializedQueryResult::Collection() {
	return ShellColumnDataCollection(result->Collection());
}

duckdb::MaterializedQueryResult &ShellMaterializedQueryResult::GetResult() {
	return *result;
}

} // namespace duckdb_shell
