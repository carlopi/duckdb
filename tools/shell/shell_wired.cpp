#include "shell_prepared_statement_wired.hpp"
#include "shell_query_result_wired.hpp"
#include "shell_connection_wired.hpp"
#include "shell_duckdb_wired.hpp"
#include "shell_db_config.hpp"

namespace duckdb_shell {
using duckdb::make_uniq;

static const duckdb::string WIRED_STUB_ERROR = "wire mode stub - not connected";
static const duckdb::vector<duckdb::string> EMPTY_NAMES;
static const duckdb::vector<duckdb::LogicalType> EMPTY_TYPES;

// ===== ShellPreparedStatementWired =====

ShellPreparedStatementWired::ShellPreparedStatementWired() {
}

ShellPreparedStatementWired::~ShellPreparedStatementWired() {
}

bool ShellPreparedStatementWired::HasError() const {
	// TODO: wire mode
	return true;
}

const string &ShellPreparedStatementWired::GetError() const {
	// TODO: wire mode
	return WIRED_STUB_ERROR;
}

unique_ptr<ShellQueryResult> ShellPreparedStatementWired::Execute(vector<duckdb::Value> &values) {
	// TODO: wire mode
	return make_uniq<ShellQueryResultWired>();
}

// ===== ShellQueryResultWired =====

ShellQueryResultWired::ShellQueryResultWired() {
}

ShellQueryResultWired::~ShellQueryResultWired() {
}

bool ShellQueryResultWired::HasError() const {
	// TODO: wire mode
	return true;
}

const duckdb::string &ShellQueryResultWired::GetError() const {
	// TODO: wire mode
	return WIRED_STUB_ERROR;
}

duckdb::StatementReturnType ShellQueryResultWired::GetReturnType() const {
	// TODO: wire mode
	return duckdb::StatementReturnType::NOTHING;
}

duckdb::QueryResultType ShellQueryResultWired::GetResultType() const {
	// TODO: wire mode
	return duckdb::QueryResultType::MATERIALIZED_RESULT;
}

idx_t ShellQueryResultWired::ColumnCount() const {
	// TODO: wire mode
	return 0;
}

const duckdb::vector<duckdb::string> &ShellQueryResultWired::Names() const {
	// TODO: wire mode
	return EMPTY_NAMES;
}

const duckdb::vector<duckdb::LogicalType> &ShellQueryResultWired::Types() const {
	// TODO: wire mode
	return EMPTY_TYPES;
}

duckdb::unique_ptr<duckdb::DataChunk> ShellQueryResultWired::Fetch() {
	// TODO: wire mode
	return nullptr;
}

ShellQueryResult::iterator ShellQueryResultWired::begin() {
	// TODO: wire mode
	return ShellQueryResult::iterator(nullptr);
}

ShellQueryResult::iterator ShellQueryResultWired::end() {
	return ShellQueryResult::iterator(nullptr);
}

// ===== ShellMaterializedQueryResultWired =====

ShellMaterializedQueryResultWired::ShellMaterializedQueryResultWired() {
}

ShellMaterializedQueryResultWired::~ShellMaterializedQueryResultWired() {
}

bool ShellMaterializedQueryResultWired::HasError() const {
	// TODO: wire mode
	return true;
}

const duckdb::string &ShellMaterializedQueryResultWired::GetError() const {
	// TODO: wire mode
	return WIRED_STUB_ERROR;
}

duckdb::StatementReturnType ShellMaterializedQueryResultWired::GetReturnType() const {
	// TODO: wire mode
	return duckdb::StatementReturnType::NOTHING;
}

duckdb::QueryResultType ShellMaterializedQueryResultWired::GetResultType() const {
	// TODO: wire mode
	return duckdb::QueryResultType::MATERIALIZED_RESULT;
}

idx_t ShellMaterializedQueryResultWired::ColumnCount() const {
	// TODO: wire mode
	return 0;
}

const duckdb::vector<duckdb::string> &ShellMaterializedQueryResultWired::Names() const {
	// TODO: wire mode
	return EMPTY_NAMES;
}

const duckdb::vector<duckdb::LogicalType> &ShellMaterializedQueryResultWired::Types() const {
	// TODO: wire mode
	return EMPTY_TYPES;
}

duckdb::unique_ptr<duckdb::DataChunk> ShellMaterializedQueryResultWired::Fetch() {
	// TODO: wire mode
	return nullptr;
}

ShellQueryResult::iterator ShellMaterializedQueryResultWired::begin() {
	// TODO: wire mode
	return ShellQueryResult::iterator(nullptr);
}

ShellQueryResult::iterator ShellMaterializedQueryResultWired::end() {
	return ShellQueryResult::iterator(nullptr);
}

idx_t ShellMaterializedQueryResultWired::RowCount() const {
	// TODO: wire mode
	return 0;
}

ShellColumnDataCollection ShellMaterializedQueryResultWired::Collection() {
	// TODO: wire mode — need a way to construct an empty ShellColumnDataCollection
	throw duckdb::InternalException("ShellMaterializedQueryResultWired::Collection() not implemented");
}

// ===== ShellConnectionWired =====

ShellConnectionWired::ShellConnectionWired() {
}

ShellConnectionWired::~ShellConnectionWired() {
}

unique_ptr<ShellMaterializedQueryResult> ShellConnectionWired::Query(const string &sql) {
	// TODO: wire mode
	return make_uniq<ShellMaterializedQueryResultWired>();
}

unique_ptr<ShellQueryResult> ShellConnectionWired::SendQuery(const string &query) {
	// TODO: wire mode
	return make_uniq<ShellQueryResultWired>();
}

unique_ptr<ShellQueryResult> ShellConnectionWired::SendQuery(unique_ptr<duckdb::SQLStatement> statement,
                                                             duckdb::QueryParameters parameters) {
	// TODO: wire mode
	return make_uniq<ShellQueryResultWired>();
}

unique_ptr<ShellQueryResult> ShellConnectionWired::SendQuery(unique_ptr<duckdb::SQLStatement> statement) {
	// TODO: wire mode
	return make_uniq<ShellQueryResultWired>();
}

vector<unique_ptr<duckdb::SQLStatement>> ShellConnectionWired::ExtractStatements(const string &sql) {
	// TODO: wire mode
	return {};
}

unique_ptr<ShellPreparedStatement> ShellConnectionWired::Prepare(const string &sql) {
	// TODO: wire mode
	return make_uniq<ShellPreparedStatementWired>();
}

bool ShellConnectionWired::IsAutoCommit() {
	// TODO: wire mode
	return true;
}

void ShellConnectionWired::BeginTransaction() {
	// TODO: wire mode
}

void ShellConnectionWired::Commit() {
	// TODO: wire mode
}

void ShellConnectionWired::Rollback() {
	// TODO: wire mode
}

void ShellConnectionWired::Interrupt() {
	// TODO: wire mode
}

void ShellConnectionWired::ClearInterrupt() {
	// TODO: wire mode
}

unique_ptr<duckdb::TableDescription> ShellConnectionWired::TableInfo(const string &table_name) {
	// TODO: wire mode
	return nullptr;
}

unique_ptr<duckdb::DataChunk> ShellConnectionWired::CastToVarchar(duckdb::DataChunk &chunk,
                                                                   bool complex_objects_as_json) {
	// TODO: wire mode
	return nullptr;
}

// ===== ShellDuckDBWired =====

ShellDuckDBWired::ShellDuckDBWired(const char *path, ShellDBConfig &config) {
	// TODO: wire mode — establish connection to remote server
}

ShellDuckDBWired::~ShellDuckDBWired() {
}

bool ShellDuckDBWired::IsOpen() const {
	// TODO: wire mode
	return false;
}

void ShellDuckDBWired::Reset() {
	// TODO: wire mode
}

unique_ptr<ShellConnection> ShellDuckDBWired::CreateConnection() {
	// TODO: wire mode
	return make_uniq<ShellConnectionWired>();
}

#ifdef DUCKDB_SHELL_WIRE_MODE
unique_ptr<ShellDuckDB> ShellDuckDB::Create(const char *path, ShellDBConfig &config) {
	return make_uniq<ShellDuckDBWired>(path, config);
}
#endif

} // namespace duckdb_shell
