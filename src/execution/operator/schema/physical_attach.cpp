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

	// Attach mutates the info (name resolution, path prefix stripping, injected provision options), so
	// work on a copy: the plan-owned info must stay pristine for a re-executed prepared statement.
	auto attach_info = info->Copy();

	// `WITH EXTERNAL RESOURCE ... ATTACH`: provision the resource first, then attach its endpoint under
	// this alias with the deleter bound so DETACH tears it down.
	LaunchedResource launched;
	string resource_type, resource_name;
	if (attach_info->external_resource) {
		auto &external_resource = *attach_info->external_resource;
		resource_type = external_resource.provider;
		resource_name = external_resource.alias.GetIdentifierName();
		launched = ProvisionExternalResource(context.client, external_resource.provider, external_resource.params,
		                                     resource_name);
		ApplyLaunchedResource(launched, *attach_info);
	}

	// construct the options
	AttachOptions options(attach_info->options, config.options.access_mode);
	options.deleter_function = launched.deleter_function;
	options.deleter_payload = launched.deleter_payload;
	options.deleter_resource_type = resource_type;
	options.deleter_resource_name = resource_name;

	// get the name and path of the database
	auto &name = attach_info->name;
	auto &path = attach_info->path;
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
		db_manager.AttachDatabase(context.client, *attach_info, options);
	} catch (...) {
		// Compensating teardown (best-effort): the attach failed, so nothing owns the provisioned
		// resource. The attach error takes precedence over a teardown failure.
		ResourceDeleter(DatabaseInstance::GetDatabase(context.client), launched.deleter_function,
		                launched.deleter_payload, resource_type, resource_name)
		    .TryDelete();
		throw;
	}
	return SourceResultType::FINISHED;
}

} // namespace duckdb
