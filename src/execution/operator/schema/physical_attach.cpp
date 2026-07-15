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
	// this alias with the deleter bound so DETACH tears it down.
	LaunchedResource launched;
	if (info->external_resource) {
		auto &external_resource = *info->external_resource;
		launched = ProvisionExternalResource(context.client, external_resource.provider, external_resource.params);
		ApplyLaunchedResource(launched, *info);
	}

	// construct the options
	AttachOptions options(info->options, config.options.access_mode);
	options.deleter_function = launched.deleter_function;
	options.deleter_payload = launched.deleter_payload;

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
	try {
		db_manager.AttachDatabase(context.client, *info, options);
	} catch (...) {
		// Compensating teardown (best-effort): the attach failed, so nothing owns the provisioned
		// resource. The attach error takes precedence over a teardown failure.
		ResourceDeleter(DatabaseInstance::GetDatabase(context.client), launched.deleter_function,
		                launched.deleter_payload)
		    .TryDelete();
		throw;
	}
	return SourceResultType::FINISHED;
}

} // namespace duckdb
