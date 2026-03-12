#include "shell_query_result_local.hpp"

namespace duckdb_shell {

// ===== ShellQueryResultLocal =====

ShellQueryResultLocal::ShellQueryResultLocal(duckdb::unique_ptr<duckdb::QueryResult> result)
    : result(std::move(result)) {
}

ShellQueryResultLocal::~ShellQueryResultLocal() {
}

bool ShellQueryResultLocal::HasError() const {
	return result->HasError();
}

const duckdb::string &ShellQueryResultLocal::GetError() const {
	return result->GetError();
}

duckdb::StatementReturnType ShellQueryResultLocal::GetReturnType() const {
	return result->properties.return_type;
}

duckdb::QueryResultType ShellQueryResultLocal::GetResultType() const {
	return result->type;
}

idx_t ShellQueryResultLocal::ColumnCount() const {
	return result->ColumnCount();
}

const duckdb::vector<duckdb::string> &ShellQueryResultLocal::Names() const {
	return result->names;
}

const duckdb::vector<duckdb::LogicalType> &ShellQueryResultLocal::Types() const {
	return result->types;
}

duckdb::unique_ptr<duckdb::DataChunk> ShellQueryResultLocal::Fetch() {
	return result->Fetch();
}

ShellQueryResult::iterator ShellQueryResultLocal::begin() {
	return result->begin();
}

ShellQueryResult::iterator ShellQueryResultLocal::end() {
	return result->end();
}

duckdb::QueryResult &ShellQueryResultLocal::GetResult() {
	return *result;
}

duckdb::unique_ptr<duckdb::MaterializedQueryResult> ShellQueryResultLocal::TakeMaterialized() {
	return duckdb::unique_ptr_cast<duckdb::QueryResult, duckdb::MaterializedQueryResult>(std::move(result));
}

// ===== ShellMaterializedQueryResultLocal =====

ShellMaterializedQueryResultLocal::ShellMaterializedQueryResultLocal(
    duckdb::unique_ptr<duckdb::MaterializedQueryResult> result)
    : result(std::move(result)) {
}

ShellMaterializedQueryResultLocal::~ShellMaterializedQueryResultLocal() {
}

bool ShellMaterializedQueryResultLocal::HasError() const {
	return result->HasError();
}

const duckdb::string &ShellMaterializedQueryResultLocal::GetError() const {
	return result->GetError();
}

duckdb::StatementReturnType ShellMaterializedQueryResultLocal::GetReturnType() const {
	return result->properties.return_type;
}

duckdb::QueryResultType ShellMaterializedQueryResultLocal::GetResultType() const {
	return result->type;
}

idx_t ShellMaterializedQueryResultLocal::ColumnCount() const {
	return result->ColumnCount();
}

const duckdb::vector<duckdb::string> &ShellMaterializedQueryResultLocal::Names() const {
	return result->names;
}

const duckdb::vector<duckdb::LogicalType> &ShellMaterializedQueryResultLocal::Types() const {
	return result->types;
}

duckdb::unique_ptr<duckdb::DataChunk> ShellMaterializedQueryResultLocal::Fetch() {
	return result->Fetch();
}

ShellQueryResult::iterator ShellMaterializedQueryResultLocal::begin() {
	return result->begin();
}

ShellQueryResult::iterator ShellMaterializedQueryResultLocal::end() {
	return result->end();
}

idx_t ShellMaterializedQueryResultLocal::RowCount() const {
	return result->RowCount();
}

ShellColumnDataCollection ShellMaterializedQueryResultLocal::Collection() {
	return ShellColumnDataCollection(result->Collection());
}

duckdb::MaterializedQueryResult &ShellMaterializedQueryResultLocal::GetMaterializedResult() {
	return *result;
}

} // namespace duckdb_shell
