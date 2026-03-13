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

#ifdef DUCKDB_WIRE_STUBS_UNREACHABLE
// With LTO, marking stubs as unreachable lets the linker DCE entire
// call chains that lead here — significantly reducing binary size.
[[noreturn]] static void WireStub(const char *) {
	__builtin_unreachable();
}
#else
[[noreturn]] static void WireStub(const char *name) {
	throw InternalException("%s: not available in wire mode", name);
}
#endif

// ===== Catalog =====

const string &Catalog::GetName() const {
	WireStub("Catalog::GetName");
}

bool Catalog::IsSystemCatalog() const {
	return false;
}

optional_ptr<CatalogEntry> Catalog::GetEntry(ClientContext &, CatalogType, const string &, const string &,
                                             OnEntryNotFound) {
	WireStub("Catalog::GetEntry");
}

optional_ptr<CatalogEntry> Catalog::GetEntry(ClientContext &, const string &, const string &, const EntryLookupInfo &,
                                             OnEntryNotFound) {
	WireStub("Catalog::GetEntry");
}

Catalog &Catalog::GetSystemCatalog(ClientContext &) {
	WireStub("Catalog::GetSystemCatalog");
}

optional_ptr<CatalogEntry> CatalogEntryRetriever::GetEntry(const string &, const string &, const EntryLookupInfo &,
                                                           OnEntryNotFound) {
	WireStub("CatalogEntryRetriever::GetEntry");
}

string CatalogTypeToString(CatalogType) {
	return "unknown";
}

string TableCatalogEntry::ColumnNamesToSQL(const ColumnList &) {
	WireStub("TableCatalogEntry::ColumnNamesToSQL");
}

string TableCatalogEntry::ColumnsToSQL(const ColumnList &, const vector<unique_ptr<Constraint>> &) {
	WireStub("TableCatalogEntry::ColumnsToSQL");
}

// ===== DBConfig =====

DBConfig &DBConfig::GetConfig(ClientContext &) {
	WireStub("DBConfig::GetConfig");
}

const DBConfig &DBConfig::GetConfig(const ClientContext &) {
	WireStub("DBConfig::GetConfig");
}

DBConfig &DBConfig::GetConfig(DatabaseInstance &) {
	WireStub("DBConfig::GetConfig");
}

void DBConfig::SetOptionByName(const string &, const Value &) {
	WireStub("DBConfig::SetOptionByName");
}

CastFunctionSet &DBConfig::GetCastFunctions() {
	WireStub("DBConfig::GetCastFunctions");
}

ExtensionCallbackManager &DBConfig::GetCallbackManager() {
	WireStub("DBConfig::GetCallbackManager");
}

const ExtensionCallbackManager &DBConfig::GetCallbackManager() const {
	WireStub("DBConfig::GetCallbackManager");
}

DuckDB::~DuckDB() {
}

Connection::~Connection() {
}

LogManager &DatabaseInstance::GetLogManager() const {
	WireStub("DatabaseInstance::GetLogManager");
}

StorageManager &AttachedDatabase::GetStorageManager() {
	WireStub("AttachedDatabase::GetStorageManager");
}

// ===== CastFunctionSet =====

CastFunctionSet::CastFunctionSet() {
}

CastFunctionSet &CastFunctionSet::Get(ClientContext &) {
	WireStub("CastFunctionSet::Get");
}

BoundCastInfo CastFunctionSet::GetCastFunction(const LogicalType &, const LogicalType &, GetCastFunctionInput &) {
	WireStub("CastFunctionSet::GetCastFunction");
}

int64_t CastFunctionSet::ImplicitCastCost(ClientContext &, const LogicalType &, const LogicalType &) {
	WireStub("CastFunctionSet::ImplicitCastCost");
}

// ===== TypeManager =====

LogicalType TypeManager::ParseLogicalType(const string &, ClientContext &) const {
	WireStub("TypeManager::ParseLogicalType");
}

TypeManager &TypeManager::Get(ClientContext &) {
	WireStub("TypeManager::Get");
}

// ===== Binder / ExpressionBinder =====

shared_ptr<Binder> Binder::CreateBinder(ClientContext &, optional_ptr<Binder>, BinderType) {
	WireStub("Binder::CreateBinder");
}

void Binder::BindCreateViewInfo(CreateViewInfo &) {
	WireStub("Binder::BindCreateViewInfo");
}

ExpressionBinder::ExpressionBinder(Binder &binder, ClientContext &context, bool) : binder(binder), context(context) {
}

ExpressionBinder::~ExpressionBinder() {
}

BindResult ExpressionBinder::BindExpression(unique_ptr<ParsedExpression> &, idx_t, bool) {
	WireStub("ExpressionBinder::BindExpression");
}

BindResult ExpressionBinder::BindGroupingFunction(OperatorExpression &, idx_t) {
	WireStub("ExpressionBinder::BindGroupingFunction");
}

BindResult ExpressionBinder::BindFunction(FunctionExpression &, ScalarFunctionCatalogEntry &, idx_t) {
	WireStub("ExpressionBinder::BindFunction");
}

BindResult ExpressionBinder::BindLambdaFunction(FunctionExpression &, ScalarFunctionCatalogEntry &, idx_t) {
	WireStub("ExpressionBinder::BindLambdaFunction");
}

BindResult ExpressionBinder::BindAggregate(FunctionExpression &, AggregateFunctionCatalogEntry &, idx_t) {
	WireStub("ExpressionBinder::BindAggregate");
}

BindResult ExpressionBinder::BindUnnest(FunctionExpression &, idx_t, bool) {
	WireStub("ExpressionBinder::BindUnnest");
}

BindResult ExpressionBinder::BindMacro(FunctionExpression &, ScalarMacroCatalogEntry &, idx_t,
                                       unique_ptr<ParsedExpression> &) {
	WireStub("ExpressionBinder::BindMacro");
}

unique_ptr<ParsedExpression> ExpressionBinder::GetSQLValueFunction(const string &) {
	WireStub("ExpressionBinder::GetSQLValueFunction");
}

string ExpressionBinder::UnsupportedAggregateMessage() {
	WireStub("ExpressionBinder::UnsupportedAggregateMessage");
}

string ExpressionBinder::UnsupportedUnnestMessage() {
	WireStub("ExpressionBinder::UnsupportedUnnestMessage");
}

void ExpressionBinder::ThrowIfUnnestInLambda(const ColumnBinding &) {
}

unique_ptr<ParsedExpression> ExpressionBinder::QualifyColumnName(ColumnRefExpression &, ErrorData &) {
	WireStub("ExpressionBinder::QualifyColumnName");
}

bool Binding::HasMatchingBinding(const string &) {
	WireStub("Binding::HasMatchingBinding");
}

// ===== DefaultTypeGenerator =====

LogicalTypeId DefaultTypeGenerator::GetDefaultType(const string &) {
	return LogicalTypeId::INVALID;
}

LogicalType DefaultTypeGenerator::TryDefaultBind(const string &, const vector<pair<string, Value>> &) {
	return LogicalType::INVALID;
}

// ===== HandleCastError =====

void HandleCastError::AssignError(const string &, CastParameters &) {
	WireStub("HandleCastError::AssignError");
}

void HandleCastError::AssignError(const string &, string *, optional_ptr<const Expression>, optional_idx) {
	WireStub("HandleCastError::AssignError");
}

// ===== Reservoir / CSV =====

void ReservoirChunk::Serialize(Serializer &) const {
	WireStub("ReservoirChunk::Serialize");
}

unique_ptr<ReservoirChunk> ReservoirChunk::Deserialize(Deserializer &) {
	WireStub("ReservoirChunk::Deserialize");
}

CSVOption<char> CSVReaderOptions::GetSingleByteDelimiter() const {
	WireStub("CSVReaderOptions::GetSingleByteDelimiter");
}

CSVOption<string> CSVReaderOptions::GetMultiByteDelimiter() const {
	WireStub("CSVReaderOptions::GetMultiByteDelimiter");
}

SerializedCSVReaderOptions::SerializedCSVReaderOptions(CSVOption<char>, const CSVOption<string> &) {
}

// ===== Logging =====

shared_ptr<Logger> LogManager::GlobalLoggerReference() {
	return nullptr;
}

Logger &Logger::Get(const ClientContext &) {
	WireStub("Logger::Get");
}

Logger &Logger::Get(const DatabaseInstance &) {
	WireStub("Logger::Get");
}

Logger &Logger::Get(const shared_ptr<Logger> &) {
	WireStub("Logger::Get");
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
	WireStub("Allocator::Get");
}

optional_ptr<ClientContext> FileOpener::TryGetClientContext(optional_ptr<FileOpener>) {
	return nullptr;
}

ClientData &ClientData::Get(ClientContext &) {
	WireStub("ClientData::Get");
}

void QueryProfiler::AddToCounter(MetricType, idx_t) {
}

ManagedResultSet::ManagedResultSet() {
}

bool Settings::TryGetSettingInternal(const ClientContext &, idx_t, Value &) {
	WireStub("Settings::TryGetSettingInternal");
}

void ExpressionIterator::EnumerateChildren(const Expression &, const std::function<void(const Expression &)> &) {
	WireStub("ExpressionIterator::EnumerateChildren");
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
	WireStub("Glob");
}

string FSSTPrimitives::DecompressValue(void *, const char *, idx_t, vector<uint8_t> &) {
	WireStub("FSSTPrimitives::DecompressValue");
}

string Function::CallToString(const string &, const string &, const string &, const vector<LogicalType> &,
                              const LogicalType &) {
	return "unknown_function()";
}

unique_ptr<Expression> Expression::Deserialize(Deserializer &) {
	WireStub("Expression::Deserialize");
}

void HivePartitioning::ApplyFiltersToFileList(ClientContext &, vector<OpenFileInfo> &, vector<unique_ptr<Expression>> &,
                                              const HivePartitioningFilterInfo &, MultiFilePushdownInfo &) {
	WireStub("HivePartitioning::ApplyFiltersToFileList");
}

HivePartitioningIndex::HivePartitioningIndex(string, idx_t) {
}

const vector<ColumnIndex> &LogicalGet::GetColumnIds() const {
	WireStub("LogicalGet::GetColumnIds");
}

StrpTimeFormat::StrpTimeFormat() {
}

StrpTimeFormat::StrpTimeFormat(const string &) {
}

// ===== VectorOperations selection =====

idx_t VectorOperations::Equals(Vector &, Vector &, optional_ptr<const SelectionVector>, idx_t,
                               optional_ptr<SelectionVector>, optional_ptr<SelectionVector>,
                               optional_ptr<ValidityMask>) {
	WireStub("VectorOperations::Equals");
}

idx_t VectorOperations::NotEquals(Vector &, Vector &, optional_ptr<const SelectionVector>, idx_t,
                                  optional_ptr<SelectionVector>, optional_ptr<SelectionVector>,
                                  optional_ptr<ValidityMask>) {
	WireStub("VectorOperations::NotEquals");
}

idx_t VectorOperations::GreaterThan(Vector &, Vector &, optional_ptr<const SelectionVector>, idx_t,
                                    optional_ptr<SelectionVector>, optional_ptr<SelectionVector>,
                                    optional_ptr<ValidityMask>) {
	WireStub("VectorOperations::GreaterThan");
}

idx_t VectorOperations::GreaterThanEquals(Vector &, Vector &, optional_ptr<const SelectionVector>, idx_t,
                                          optional_ptr<SelectionVector>, optional_ptr<SelectionVector>,
                                          optional_ptr<ValidityMask>) {
	WireStub("VectorOperations::GreaterThanEquals");
}

void VariantUtils::UnshredVariantData(Vector &, Vector &, idx_t) {
	WireStub("VariantUtils::UnshredVariantData");
}

unique_ptr<TableFilter> TableFilter::Deserialize(Deserializer &) {
	WireStub("TableFilter::Deserialize");
}

// ===== DummyBinding =====

Binding::Binding(BindingType binding_type, BindingAlias alias, vector<LogicalType> types, vector<string> names,
                 TableIndex index)
    : binding_type(binding_type), alias(std::move(alias)), types(std::move(types)), names(std::move(names)),
      index(index) {
}

ErrorData Binding::ColumnNotFoundError(const string &) const {
	WireStub("Binding::ColumnNotFoundError");
}

BindResult Binding::Bind(ColumnRefExpression &, idx_t) {
	WireStub("Binding::Bind");
}

optional_ptr<StandardEntry> Binding::GetStandardEntry() {
	return nullptr;
}

DummyBinding::DummyBinding(vector<LogicalType> types, vector<string> names, string dummy_name)
    : Binding(BindingType::DUMMY, BindingAlias(dummy_name), std::move(types), std::move(names), TableIndex()) {
}

BindResult DummyBinding::Bind(ColumnRefExpression &, idx_t) {
	WireStub("DummyBinding::Bind");
}

// ===== TableFilterSet =====

TableFilterSet::ConstTableFilterIteratorEntry::ConstTableFilterIteratorEntry(
    map<idx_t, unique_ptr<TableFilter>>::const_iterator it)
    : iterator(it) {
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
	WireStub("ReservoirSample::AddToReservoir");
}

unique_ptr<BlockingSample> ReservoirSample::Copy() const {
	WireStub("ReservoirSample::Copy");
}

void ReservoirSample::Finalize() {
}

unique_ptr<DataChunk> ReservoirSample::GetChunk() {
	WireStub("ReservoirSample::GetChunk");
}

ReservoirSamplePercentage::ReservoirSamplePercentage(double percentage, int64_t seed)
    : BlockingSample(seed), allocator(Allocator::DefaultAllocator()), sample_percentage(percentage) {
}

void ReservoirSamplePercentage::AddToReservoir(DataChunk &) {
	WireStub("ReservoirSamplePercentage::AddToReservoir");
}

unique_ptr<BlockingSample> ReservoirSamplePercentage::Copy() const {
	WireStub("ReservoirSamplePercentage::Copy");
}

void ReservoirSamplePercentage::Finalize() {
}

unique_ptr<DataChunk> ReservoirSamplePercentage::GetChunk() {
	WireStub("ReservoirSamplePercentage::GetChunk");
}

// ===== Expression =====

void Expression::Serialize(Serializer &) const {
	WireStub("Expression::Serialize");
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
	WireStub("BoundColumnRefExpression::Copy");
}

void BoundColumnRefExpression::Serialize(Serializer &) const {
	WireStub("BoundColumnRefExpression::Serialize");
}

// ===== TableCatalogEntry =====

constexpr const char *TableCatalogEntry::Name;

} // namespace duckdb
