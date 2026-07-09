#include "duckdb/execution/operator/schema/physical_attach.hpp"

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/execution/operator/helper/launch_external_resource.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/database_manager.hpp"
#include "duckdb/main/extension_helper.hpp"
#include "duckdb/parser/parsed_data/attach_info.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/main/database_path_and_type.hpp"

namespace duckdb {

//===--------------------------------------------------------------------===//
// Source
//===--------------------------------------------------------------------===//
SourceResultType PhysicalAttach::GetDataInternal(ExecutionContext &context, DataChunk &chunk,
                                                 OperatorSourceInput &input) const {
	auto &config = DBConfig::GetConfig(context.client);

	// `WITH EXTERNAL RESOURCE ... ATTACH`: provision the resource first, then attach its endpoint under
	// this alias and bind the deleter so DETACH tears it down.
	string deleter_function;
	Value deleter_payload;
	if (info->external_resource) {
		auto &external_resource = *info->external_resource;
		auto launched = ProvisionExternalResource(context.client, external_resource.provider, external_resource.params);
		ApplyLaunchedResource(launched, *info);
		deleter_function = launched.deleter_function;
		deleter_payload = launched.deleter_payload;
	}

	// construct the options
	AttachOptions options(info->options, config.options.access_mode);

	// get the name and path of the database
	auto &name = info->name;
	auto &path = info->path;
	// preserve the verbatim path before extension-prefix stripping
	options.original_path = path;
	if (options.db_type.empty()) {
		DBPathAndType::ExtractExtensionPrefix(path, options.db_type);
	}
	if (name.empty()) {
		auto &fs = FileSystem::GetFileSystem(context.client);
		name = Identifier(AttachedDatabase::ExtractDatabaseName(path, fs));
	}

	// check ATTACH IF NOT EXISTS
	auto &db_manager = DatabaseManager::Get(context.client);
	auto attached = db_manager.AttachDatabase(context.client, *info, options);

	// Bind the deleter to the attachment so DETACH runs the resource's teardown.
	if (!deleter_function.empty() && attached) {
		attached->SetDeleter(std::move(deleter_function), std::move(deleter_payload));
	}
	return SourceResultType::FINISHED;
}

} // namespace duckdb
