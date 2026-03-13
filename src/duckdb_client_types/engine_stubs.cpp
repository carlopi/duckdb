// Wire-mode stubs for engine symbols that are transitively referenced
// by client-side code but don't have meaningful implementations without
// a local engine.

// Include order matters — some headers need others to be complete
#include "duckdb/common/operator/cast_operators.hpp"
#include "duckdb/common/file_opener.hpp"
#include "duckdb/common/fsst.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/catalog/catalog_entry_retriever.hpp"
#include "duckdb/catalog/default/default_types.hpp"
#include "duckdb/common/types/type_manager.hpp"
#include "duckdb/execution/operator/csv_scanner/csv_reader_options.hpp"
#include "duckdb/execution/reservoir_sample.hpp"
#include "duckdb/function/cast/cast_function_set.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/function/scalar/variant_utils.hpp"
#include "duckdb/function/table/read_csv.hpp"
#include "duckdb/logging/log_manager.hpp"
#include "duckdb/logging/log_type.hpp"
#include "duckdb/logging/logger.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/main/client_data.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/query_profiler.hpp"
#include "duckdb/main/settings.hpp"
#include "duckdb/planner/binder.hpp"
#include "duckdb/planner/bound_result_modifier.hpp"
#include "duckdb/planner/expression/bound_columnref_expression.hpp"
#include "duckdb/planner/expression_binder.hpp"
#include "duckdb/planner/expression_iterator.hpp"
#include "duckdb/planner/operator/logical_get.hpp"
#include "duckdb/planner/table_binding.hpp"
#include "duckdb/storage/statistics/base_statistics.hpp"
#include "duckdb/common/multi_file/multi_file_list.hpp"
#include "duckdb/common/multi_file/multi_file_data.hpp"
#include "duckdb/planner/table_filter_set.hpp"

namespace duckdb {

static const char *WIRE_STUB = "not available in wire mode";

// ===== Catalog =====

const string &Catalog::GetName() const {
	throw InternalException("Catalog::GetName: %s", WIRE_STUB);
}

bool Catalog::IsSystemCatalog() const {
	return false;
}

optional_ptr<CatalogEntry> Catalog::GetEntry(ClientContext &, CatalogType, const string &, const string &,
                                             OnEntryNotFound) {
	throw InternalException("Catalog::GetEntry: %s", WIRE_STUB);
}

optional_ptr<CatalogEntry> Catalog::GetEntry(ClientContext &, const string &, const string &,
                                             const EntryLookupInfo &, OnEntryNotFound) {
	throw InternalException("Catalog::GetEntry: %s", WIRE_STUB);
}

Catalog &Catalog::GetSystemCatalog(ClientContext &) {
	throw InternalException("Catalog::GetSystemCatalog: %s", WIRE_STUB);
}

optional_ptr<CatalogEntry> CatalogEntryRetriever::GetEntry(const string &, const string &,
                                                            const EntryLookupInfo &, OnEntryNotFound) {
	throw InternalException("CatalogEntryRetriever::GetEntry: %s", WIRE_STUB);
}

string CatalogTypeToString(CatalogType) {
	return "unknown";
}

string TableCatalogEntry::ColumnNamesToSQL(const ColumnList &) {
	throw InternalException("TableCatalogEntry: %s", WIRE_STUB);
}

string TableCatalogEntry::ColumnsToSQL(const ColumnList &, const vector<unique_ptr<Constraint>> &) {
	throw InternalException("TableCatalogEntry: %s", WIRE_STUB);
}

// ===== DBConfig =====

DBConfig &DBConfig::GetConfig(ClientContext &) {
	throw InternalException("DBConfig::GetConfig: %s", WIRE_STUB);
}

const DBConfig &DBConfig::GetConfig(const ClientContext &) {
	throw InternalException("DBConfig::GetConfig: %s", WIRE_STUB);
}

DBConfig &DBConfig::GetConfig(DatabaseInstance &) {
	throw InternalException("DBConfig::GetConfig: %s", WIRE_STUB);
}

void DBConfig::SetOptionByName(const string &, const Value &) {
	throw InternalException("DBConfig::SetOptionByName: %s", WIRE_STUB);
}

CastFunctionSet &DBConfig::GetCastFunctions() {
	throw InternalException("DBConfig::GetCastFunctions: %s", WIRE_STUB);
}

ExtensionCallbackManager &DBConfig::GetCallbackManager() {
	throw InternalException("DBConfig::GetCallbackManager: %s", WIRE_STUB);
}

const ExtensionCallbackManager &DBConfig::GetCallbackManager() const {
	throw InternalException("DBConfig::GetCallbackManager: %s", WIRE_STUB);
}

DuckDB::~DuckDB() {
}

Connection::~Connection() {
}

LogManager &DatabaseInstance::GetLogManager() const {
	throw InternalException("DatabaseInstance::GetLogManager: %s", WIRE_STUB);
}

StorageManager &AttachedDatabase::GetStorageManager() {
	throw InternalException("AttachedDatabase::GetStorageManager: %s", WIRE_STUB);
}

// ===== CastFunctionSet =====

CastFunctionSet::CastFunctionSet() {
}

CastFunctionSet &CastFunctionSet::Get(ClientContext &) {
	throw InternalException("CastFunctionSet::Get: %s", WIRE_STUB);
}

BoundCastInfo CastFunctionSet::GetCastFunction(const LogicalType &, const LogicalType &, GetCastFunctionInput &) {
	throw InternalException("CastFunctionSet::GetCastFunction: %s", WIRE_STUB);
}

int64_t CastFunctionSet::ImplicitCastCost(ClientContext &, const LogicalType &, const LogicalType &) {
	throw InternalException("CastFunctionSet::ImplicitCastCost: %s", WIRE_STUB);
}

// ===== TypeManager =====

LogicalType TypeManager::ParseLogicalType(const string &, ClientContext &) const {
	throw InternalException("TypeManager::ParseLogicalType: %s", WIRE_STUB);
}

TypeManager &TypeManager::Get(ClientContext &) {
	throw InternalException("TypeManager::Get: %s", WIRE_STUB);
}

// ===== Binder / ExpressionBinder =====

shared_ptr<Binder> Binder::CreateBinder(ClientContext &, optional_ptr<Binder>, BinderType) {
	throw InternalException("Binder::CreateBinder: %s", WIRE_STUB);
}

void Binder::BindCreateViewInfo(CreateViewInfo &) {
	throw InternalException("Binder::BindCreateViewInfo: %s", WIRE_STUB);
}

ExpressionBinder::ExpressionBinder(Binder &binder, ClientContext &context, bool)
    : binder(binder), context(context) {
}

ExpressionBinder::~ExpressionBinder() {
}

BindResult ExpressionBinder::BindExpression(unique_ptr<ParsedExpression> &, idx_t, bool) {
	throw InternalException("ExpressionBinder::BindExpression: %s", WIRE_STUB);
}

BindResult ExpressionBinder::BindGroupingFunction(OperatorExpression &, idx_t) {
	throw InternalException("ExpressionBinder::BindGroupingFunction: %s", WIRE_STUB);
}

BindResult ExpressionBinder::BindFunction(FunctionExpression &, ScalarFunctionCatalogEntry &, idx_t) {
	throw InternalException("ExpressionBinder::BindFunction: %s", WIRE_STUB);
}

BindResult ExpressionBinder::BindLambdaFunction(FunctionExpression &, ScalarFunctionCatalogEntry &, idx_t) {
	throw InternalException("ExpressionBinder::BindLambdaFunction: %s", WIRE_STUB);
}

BindResult ExpressionBinder::BindAggregate(FunctionExpression &, AggregateFunctionCatalogEntry &, idx_t) {
	throw InternalException("ExpressionBinder::BindAggregate: %s", WIRE_STUB);
}

BindResult ExpressionBinder::BindUnnest(FunctionExpression &, idx_t, bool) {
	throw InternalException("ExpressionBinder::BindUnnest: %s", WIRE_STUB);
}

BindResult ExpressionBinder::BindMacro(FunctionExpression &, ScalarMacroCatalogEntry &, idx_t,
                                        unique_ptr<ParsedExpression> &) {
	throw InternalException("ExpressionBinder::BindMacro: %s", WIRE_STUB);
}

unique_ptr<ParsedExpression> ExpressionBinder::GetSQLValueFunction(const string &) {
	throw InternalException("ExpressionBinder::GetSQLValueFunction: %s", WIRE_STUB);
}

string ExpressionBinder::UnsupportedAggregateMessage() {
	throw InternalException("ExpressionBinder::UnsupportedAggregateMessage: %s", WIRE_STUB);
}

string ExpressionBinder::UnsupportedUnnestMessage() {
	throw InternalException("ExpressionBinder::UnsupportedUnnestMessage: %s", WIRE_STUB);
}

void ExpressionBinder::ThrowIfUnnestInLambda(const ColumnBinding &) {
}

unique_ptr<ParsedExpression> ExpressionBinder::QualifyColumnName(ColumnRefExpression &, ErrorData &) {
	throw InternalException("ExpressionBinder::QualifyColumnName: %s", WIRE_STUB);
}

bool Binding::HasMatchingBinding(const string &) {
	throw InternalException("Binding::HasMatchingBinding: %s", WIRE_STUB);
}

// ===== DefaultTypeGenerator =====

LogicalTypeId DefaultTypeGenerator::GetDefaultType(const string &) {
	return LogicalTypeId::INVALID;
}

LogicalType DefaultTypeGenerator::TryDefaultBind(const string &,
                                                  const vector<pair<string, Value>> &) {
	return LogicalType::INVALID;
}

// ===== HandleCastError =====

void HandleCastError::AssignError(const string &, CastParameters &) {
	throw InternalException("HandleCastError::AssignError: %s", WIRE_STUB);
}

void HandleCastError::AssignError(const string &, string *, optional_ptr<const Expression>, optional_idx) {
	throw InternalException("HandleCastError::AssignError: %s", WIRE_STUB);
}

// ===== Reservoir / CSV =====

void ReservoirChunk::Serialize(Serializer &) const {
	throw InternalException("ReservoirChunk::Serialize: %s", WIRE_STUB);
}

unique_ptr<ReservoirChunk> ReservoirChunk::Deserialize(Deserializer &) {
	throw InternalException("ReservoirChunk::Deserialize: %s", WIRE_STUB);
}

CSVOption<char> CSVReaderOptions::GetSingleByteDelimiter() const {
	throw InternalException("CSVReaderOptions::GetSingleByteDelimiter: %s", WIRE_STUB);
}

CSVOption<string> CSVReaderOptions::GetMultiByteDelimiter() const {
	throw InternalException("CSVReaderOptions::GetMultiByteDelimiter: %s", WIRE_STUB);
}

SerializedCSVReaderOptions::SerializedCSVReaderOptions(CSVOption<char>, const CSVOption<string> &) {
}

// ===== Logging =====

shared_ptr<Logger> LogManager::GlobalLoggerReference() {
	return nullptr;
}

Logger &Logger::Get(const ClientContext &) {
	throw InternalException("Logger::Get: %s", WIRE_STUB);
}

Logger &Logger::Get(const DatabaseInstance &) {
	throw InternalException("Logger::Get: %s", WIRE_STUB);
}

Logger &Logger::Get(const shared_ptr<Logger> &) {
	throw InternalException("Logger::Get: %s", WIRE_STUB);
}

void Logger::WriteLog(const char *, LogLevel, const string &) {
}

string FileSystemLogType::ConstructLogMessage(const FileHandle &, const string &) {
	return "";
}

string FileSystemLogType::ConstructLogMessage(const FileHandle &, const string &, int64_t, idx_t) {
	return "";
}

// ===== Various =====

Allocator &Allocator::Get(ClientContext &) {
	throw InternalException("Allocator::Get(ClientContext&): %s", WIRE_STUB);
}

optional_ptr<ClientContext> FileOpener::TryGetClientContext(optional_ptr<FileOpener>) {
	return nullptr;
}

ClientData &ClientData::Get(ClientContext &) {
	throw InternalException("ClientData::Get: %s", WIRE_STUB);
}

void QueryProfiler::AddToCounter(MetricType, idx_t) {
}

ManagedResultSet::ManagedResultSet() {
}

bool Settings::TryGetSettingInternal(const ClientContext &, idx_t, Value &) {
	throw InternalException("Settings::TryGetSettingInternal: %s", WIRE_STUB);
}

void ExpressionIterator::EnumerateChildren(const Expression &, const std::function<void(const Expression &)> &) {
	throw InternalException("ExpressionIterator::EnumerateChildren: %s", WIRE_STUB);
}

BaseStatistics::~BaseStatistics() {
}

BoundResultModifier::BoundResultModifier(ResultModifierType type) : type(type) {
}

BoundResultModifier::~BoundResultModifier() {
}

BoundOrderModifier::BoundOrderModifier() : BoundResultModifier(ResultModifierType::ORDER_MODIFIER) {
}

BoundOrderByNode::BoundOrderByNode(OrderType, OrderByNullType, unique_ptr<Expression>) {
}

BoundLimitNode::BoundLimitNode(LimitNodeType, idx_t, double, unique_ptr<Expression>) {
}

BoundColumnRefExpression::BoundColumnRefExpression(LogicalType type, ColumnBinding binding_p, idx_t depth_p)
    : Expression(ExpressionType::BOUND_COLUMN_REF, ExpressionClass::BOUND_COLUMN_REF, std::move(type)),
      binding(binding_p), depth(depth_p) {
}

ColumnBinding::ColumnBinding() {
}

ColumnBinding::ColumnBinding(TableIndex table_index_p, ProjectionIndex column_index_p)
    : table_index(table_index_p), column_index(column_index_p) {
}

StorageIndex::StorageIndex() {
}

bool Glob(const char *, idx_t, const char *, idx_t, bool) {
	throw InternalException("Glob: %s", WIRE_STUB);
}

string FSSTPrimitives::DecompressValue(void *, const char *, idx_t, vector<uint8_t> &) {
	throw InternalException("FSSTPrimitives::DecompressValue: %s", WIRE_STUB);
}

string Function::CallToString(const string &, const string &, const string &, const vector<LogicalType> &,
                               const LogicalType &) {
	return "unknown_function()";
}

unique_ptr<Expression> Expression::Deserialize(Deserializer &) {
	throw InternalException("Expression::Deserialize: %s", WIRE_STUB);
}

void HivePartitioning::ApplyFiltersToFileList(ClientContext &, vector<OpenFileInfo> &,
                                               vector<unique_ptr<Expression>> &,
                                               const HivePartitioningFilterInfo &, MultiFilePushdownInfo &) {
	throw InternalException("HivePartitioning: %s", WIRE_STUB);
}

HivePartitioningIndex::HivePartitioningIndex(string, idx_t) {
}

const vector<ColumnIndex> &LogicalGet::GetColumnIds() const {
	throw InternalException("LogicalGet: %s", WIRE_STUB);
}

StrpTimeFormat::StrpTimeFormat() {
}

StrpTimeFormat::StrpTimeFormat(const string &) {
}

// ===== VectorOperations selection =====

idx_t VectorOperations::Equals(Vector &, Vector &, optional_ptr<const SelectionVector>, idx_t,
                               optional_ptr<SelectionVector>, optional_ptr<SelectionVector>,
                               optional_ptr<ValidityMask>) {
	throw InternalException("VectorOperations::Equals: %s", WIRE_STUB);
}

idx_t VectorOperations::NotEquals(Vector &, Vector &, optional_ptr<const SelectionVector>, idx_t,
                                  optional_ptr<SelectionVector>, optional_ptr<SelectionVector>,
                                  optional_ptr<ValidityMask>) {
	throw InternalException("VectorOperations::NotEquals: %s", WIRE_STUB);
}

idx_t VectorOperations::GreaterThan(Vector &, Vector &, optional_ptr<const SelectionVector>, idx_t,
                                    optional_ptr<SelectionVector>, optional_ptr<SelectionVector>,
                                    optional_ptr<ValidityMask>) {
	throw InternalException("VectorOperations::GreaterThan: %s", WIRE_STUB);
}

idx_t VectorOperations::GreaterThanEquals(Vector &, Vector &, optional_ptr<const SelectionVector>, idx_t,
                                          optional_ptr<SelectionVector>, optional_ptr<SelectionVector>,
                                          optional_ptr<ValidityMask>) {
	throw InternalException("VectorOperations::GreaterThanEquals: %s", WIRE_STUB);
}

void VariantUtils::UnshredVariantData(Vector &, Vector &, idx_t) {
	throw InternalException("VariantUtils: %s", WIRE_STUB);
}

unique_ptr<TableFilter> TableFilter::Deserialize(Deserializer &) {
	throw InternalException("TableFilter::Deserialize: %s", WIRE_STUB);
}

// ===== DummyBinding =====

Binding::Binding(BindingType binding_type, BindingAlias alias, vector<LogicalType> types, vector<string> names,
                 TableIndex index)
    : binding_type(binding_type), alias(std::move(alias)), types(std::move(types)), names(std::move(names)),
      index(index) {
}

ErrorData Binding::ColumnNotFoundError(const string &) const {
	throw InternalException("Binding::ColumnNotFoundError: %s", WIRE_STUB);
}

BindResult Binding::Bind(ColumnRefExpression &, idx_t) {
	throw InternalException("Binding::Bind: %s", WIRE_STUB);
}

optional_ptr<StandardEntry> Binding::GetStandardEntry() {
	return nullptr;
}

DummyBinding::DummyBinding(vector<LogicalType> types, vector<string> names, string dummy_name)
    : Binding(BindingType::DUMMY, BindingAlias(dummy_name), std::move(types), std::move(names), TableIndex()) {
}

BindResult DummyBinding::Bind(ColumnRefExpression &, idx_t) {
	throw InternalException("DummyBinding::Bind: %s", WIRE_STUB);
}

// ===== TableFilterSet =====

TableFilterSet::ConstTableFilterIteratorEntry::ConstTableFilterIteratorEntry(
    map<idx_t, unique_ptr<TableFilter>>::const_iterator it) : iterator(it) {
}

idx_t TableFilterSet::ConstTableFilterIteratorEntry::ColumnIndex() const {
	return iterator->first;
}

const TableFilter &TableFilterSet::ConstTableFilterIteratorEntry::Filter() const {
	return *iterator->second;
}

// ===== ReservoirSample =====

BaseReservoirSampling::BaseReservoirSampling() : random(0) {
}

BaseReservoirSampling::BaseReservoirSampling(int64_t seed) : random(seed) {
}

void BlockingSample::Destroy() {
}

ReservoirSample::ReservoirSample(idx_t sample_count, unique_ptr<ReservoirChunk>)
    : BlockingSample(-1), allocator(Allocator::DefaultAllocator()), sample_count(sample_count) {
}

void ReservoirSample::Destroy() {
}

void ReservoirSample::AddToReservoir(DataChunk &) {
	throw InternalException("ReservoirSample::AddToReservoir: %s", WIRE_STUB);
}

unique_ptr<BlockingSample> ReservoirSample::Copy() const {
	throw InternalException("ReservoirSample::Copy: %s", WIRE_STUB);
}

void ReservoirSample::Finalize() {
}

unique_ptr<DataChunk> ReservoirSample::GetChunk() {
	throw InternalException("ReservoirSample::GetChunk: %s", WIRE_STUB);
}

ReservoirSamplePercentage::ReservoirSamplePercentage(double percentage, int64_t seed)
    : BlockingSample(seed), allocator(Allocator::DefaultAllocator()), sample_percentage(percentage) {
}

void ReservoirSamplePercentage::AddToReservoir(DataChunk &) {
	throw InternalException("ReservoirSamplePercentage::AddToReservoir: %s", WIRE_STUB);
}

unique_ptr<BlockingSample> ReservoirSamplePercentage::Copy() const {
	throw InternalException("ReservoirSamplePercentage::Copy: %s", WIRE_STUB);
}

void ReservoirSamplePercentage::Finalize() {
}

unique_ptr<DataChunk> ReservoirSamplePercentage::GetChunk() {
	throw InternalException("ReservoirSamplePercentage::GetChunk: %s", WIRE_STUB);
}


// ===== Expression =====

void Expression::Serialize(Serializer &) const {
	throw InternalException("Expression::Serialize: %s", WIRE_STUB);
}

// ===== BoundColumnRefExpression =====

string BoundColumnRefExpression::ToString() const {
	return "bound_column_ref";
}

string BoundColumnRefExpression::GetName() const {
	return "bound_column_ref";
}

bool BoundColumnRefExpression::Equals(const BaseExpression &) const {
	return false;
}

hash_t BoundColumnRefExpression::Hash() const {
	return 0;
}

unique_ptr<Expression> BoundColumnRefExpression::Copy() const {
	throw InternalException("BoundColumnRefExpression::Copy: %s", WIRE_STUB);
}

void BoundColumnRefExpression::Serialize(Serializer &) const {
	throw InternalException("BoundColumnRefExpression::Serialize: %s", WIRE_STUB);
}

// ===== TableCatalogEntry =====

constexpr const char *TableCatalogEntry::Name;

} // namespace duckdb
