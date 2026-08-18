#include "duckdb/function/table/range.hpp"
#include "duckdb/function/function_set.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/database_manager.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/checksum.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/serializer/memory_stream.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/storage/storage_info.hpp"
#include "duckdb/storage/redirect_info.hpp"
#include "duckdb/storage/magic_bytes.hpp"

namespace duckdb {

//! Write a pointer file: a valid DuckDB container whose MainHeader carries only the redirect flag and whose two
//! DatabaseHeaders carry the redirect record. Storing the record in the DatabaseHeaders (not the MainHeader) lets a
//! later re-point swap it atomically via the h1/h2 iteration count, and rides the block-encryption path when the
//! pointer is encrypted. h1 is the active slot; h2 is a valid fallback. Mirrors the default header block layout.
static void WriteRedirectPointerFile(ClientContext &context, const string &path, const RedirectInfo &redirect,
                                     bool overwrite, StorageVersion storage_version) {
	auto &fs = FileSystem::GetFileSystem(context);

	// Inspect any existing file before writing. A redirect pointer may be replaced, but a real database or a
	// foreign file must never be clobbered: overwriting a database would stamp a pointer header over it and
	// orphan its data. So `overwrite` means "replace this pointer", never "clobber whatever is there".
	if (fs.FileExists(path)) {
		if (!overwrite) {
			throw IOException("redirect_create: file \"%s\" already exists (pass overwrite => true to replace it)",
			                  path);
		}
		RedirectInfo existing;
		auto existing_type = MagicBytes::CheckMagicBytes(context, fs, path, nullptr, &existing);
		if (!(existing_type == DataFileType::DUCKDB_FILE && existing.is_redirect)) {
			throw IOException(
			    "redirect_create: refusing to overwrite \"%s\": it is not a redirect pointer file. Delete it first "
			    "if you really mean to replace it.",
			    path);
		}
	}
	auto handle = fs.OpenFile(path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE);

	constexpr idx_t block_header = Storage::DEFAULT_BLOCK_HEADER_SIZE;
	constexpr idx_t file_header = Storage::FILE_HEADER_SIZE;
	constexpr idx_t data_size = file_header - block_header;

	auto block = make_unsafe_uniq_array<data_t>(file_header);

	// MainHeader block: carries the redirect flag + identity only. The record lives in the DatabaseHeaders below.
	memset(block.get(), 0, file_header);
	MainHeader main_header {};
	main_header.version_number = VERSION_NUMBER;
	main_header.SetRedirect();
	{
		MemoryStream ser(block.get() + block_header, data_size);
		main_header.Write(ser);
	}
	Store<uint64_t>(Checksum(block.get() + block_header, data_size), block.get());
	handle->Write(QueryContext(), block.get(), file_header, 0);

	// Two DatabaseHeaders, both carrying the redirect record. h1 is active (higher iteration); h2 is a valid fallback.
	for (idx_t i = 1; i <= 2; i++) {
		memset(block.get(), 0, file_header);
		DatabaseHeader db_header;
		db_header.iteration = (i == 1) ? 1 : 0;
		db_header.meta_block = idx_t(INVALID_BLOCK);
		db_header.free_list = idx_t(INVALID_BLOCK);
		db_header.block_count = 0;
		db_header.block_alloc_size = DEFAULT_BLOCK_ALLOC_SIZE;
		db_header.vector_size = STANDARD_VECTOR_SIZE;
		db_header.storage_compatibility = storage_version;
		db_header.redirect = redirect;
		db_header.redirect.is_redirect = true;
		MemoryStream ser(block.get() + block_header, data_size);
		db_header.Write(ser);
		Store<uint64_t>(Checksum(block.get() + block_header, data_size), block.get());
		handle->Write(QueryContext(), block.get(), file_header, file_header * i);
	}
	handle->Sync();
}

struct RedirectCreateBindData : public TableFunctionData {
	string path;
	RedirectInfo redirect;
	bool overwrite = false;
	//! Pointer files are written at >= v2.0.0 so older DuckDB versions reject them (rather than opening them as an
	//! empty database). Defaults to the earliest version that guarantees that.
	StorageVersion storage_version = StorageVersion::V2_0_0;
};

//! Derive a redirect record from a currently-attached database: capture its resolved path, storage type, and the
//! options it was attached with, so a pointer created this way reopens the same target the same way.
static void DeriveRedirectFromAttachedDatabase(ClientContext &context, const string &db_name, RedirectInfo &redirect) {
	auto &db_manager = DatabaseManager::Get(context);
	auto db = db_manager.GetDatabase(context, Identifier(db_name));
	if (!db) {
		throw BinderException("redirect_create: database \"%s\" is not attached", db_name);
	}
	auto &catalog = db->GetCatalog();
	if (catalog.InMemory()) {
		throw BinderException("redirect_create: database \"%s\" is in-memory, so there is no target to redirect to",
		                      db_name);
	}
	if (catalog.IsEncrypted()) {
		throw BinderException(
		    "redirect_create: database \"%s\" is encrypted; its key cannot be captured into a pointer file", db_name);
	}
	redirect.target = catalog.GetDBPath();
	redirect.type = catalog.GetCatalogType();
	if (db->IsReadOnly()) {
		redirect.options.push_back({"read_only", "true"});
	}
	for (auto &option : db->GetAttachOptions()) {
		redirect.options.push_back({option.first, option.second.ToString()});
	}
}

static unique_ptr<FunctionData> RedirectCreateBind(ClientContext &context, TableFunctionBindInput &input,
                                                   vector<LogicalType> &return_types, vector<Identifier> &names) {
	auto result = make_uniq<RedirectCreateBindData>();
	if (input.inputs[0].IsNull()) {
		throw BinderException("redirect_create: the pointer path cannot be NULL");
	}
	result->path = StringValue::Get(input.inputs[0]);
	result->redirect.is_redirect = true;
	result->redirect.type = "duckdb";

	// Overload A: the second positional argument names a currently-attached database to capture.
	if (input.inputs.size() == 2) {
		if (input.inputs[1].IsNull()) {
			throw BinderException("redirect_create: the source database name cannot be NULL");
		}
		DeriveRedirectFromAttachedDatabase(context, StringValue::Get(input.inputs[1]), result->redirect);
	}

	// Overload B (and shared options): explicit target via `path`/`type`/`attach_options`, plus overwrite/version.
	for (auto &param : input.named_parameters) {
		if (param.second.IsNull()) {
			continue;
		}
		if (param.first == "path") {
			result->redirect.target = StringValue::Get(param.second);
		} else if (param.first == "type") {
			result->redirect.type = StringValue::Get(param.second);
		} else if (param.first == "overwrite") {
			result->overwrite = BooleanValue::Get(param.second);
		} else if (param.first == "storage_version") {
			auto version_string = StringValue::Get(param.second);
			auto version = GetStorageVersion(version_string.c_str());
			if (version == StorageVersion::INVALID) {
				throw InvalidInputException("redirect_create: unknown storage version \"%s\"", version_string);
			}
			if (version < StorageVersion::V2_0_0) {
				throw InvalidInputException(
				    "redirect_create: storage version \"%s\" is too old for a redirect pointer file - it must be "
				    "v2.0.0 or newer so that older DuckDB versions reject the pointer instead of opening it as an "
				    "empty database",
				    version_string);
			}
			result->storage_version = version;
		} else if (param.first == "attach_options") {
			auto &entries = ListValue::GetChildren(param.second);
			for (auto &entry : entries) {
				auto &kv = StructValue::GetChildren(entry);
				result->redirect.options.push_back(
				    {StringValue::Get(kv[0].DefaultCastAs(LogicalType::VARCHAR)),
				     StringValue::Get(kv[1].DefaultCastAs(LogicalType::VARCHAR))});
			}
		}
	}

	if (result->redirect.target.empty()) {
		throw BinderException("redirect_create: no target specified - pass a second argument naming an attached "
		                      "database, or the `path` named parameter");
	}

	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("path");
	return std::move(result);
}

struct RedirectCreateState : public GlobalTableFunctionState {
	bool finished = false;
};

static unique_ptr<GlobalTableFunctionState> RedirectCreateInit(ClientContext &context, TableFunctionInitInput &input) {
	return make_uniq<RedirectCreateState>();
}

static void RedirectCreateFunc(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<RedirectCreateBindData>();
	auto &state = data_p.global_state->Cast<RedirectCreateState>();
	if (state.finished) {
		return;
	}
	WriteRedirectPointerFile(context, bind_data.path, bind_data.redirect, bind_data.overwrite,
	                         bind_data.storage_version);
	output.data[0].Append(Value(bind_data.path));
	state.finished = true;
}

void RedirectCreateFunction::RegisterFunction(BuiltinFunctions &set) {
	TableFunctionSet redirect_create("redirect_create");

	// Overload A: redirect_create('pointer.db', 'attached_db_name') - capture a currently-attached database.
	TableFunction from_database({LogicalType::VARCHAR, LogicalType::VARCHAR}, RedirectCreateFunc, RedirectCreateBind,
	                            RedirectCreateInit);
	from_database.named_parameters["overwrite"] = LogicalType::BOOLEAN;
	from_database.named_parameters["storage_version"] = LogicalType::VARCHAR;
	redirect_create.AddFunction(from_database);

	// Overload B: redirect_create('pointer.db', path := ..., type := ..., attach_options := {...}) - explicit target.
	TableFunction explicit_target({LogicalType::VARCHAR}, RedirectCreateFunc, RedirectCreateBind, RedirectCreateInit);
	explicit_target.named_parameters["path"] = LogicalType::VARCHAR;
	explicit_target.named_parameters["type"] = LogicalType::VARCHAR;
	explicit_target.named_parameters["attach_options"] = LogicalType::MAP(LogicalType::VARCHAR, LogicalType::VARCHAR);
	explicit_target.named_parameters["overwrite"] = LogicalType::BOOLEAN;
	explicit_target.named_parameters["storage_version"] = LogicalType::VARCHAR;
	redirect_create.AddFunction(explicit_target);

	set.AddFunction(redirect_create);
}

} // namespace duckdb
