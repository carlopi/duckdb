#include "duckdb/common/dl.hpp"
#include "duckdb/common/virtual_file_system.hpp"
#include "duckdb/main/extension_helper.hpp"
#include "duckdb/main/error_manager.hpp"
#include "mbedtls_wrapper.hpp"

#ifndef DUCKDB_NO_THREADS
#include <thread>
#endif // DUCKDB_NO_THREADS

#ifdef WASM_LOADABLE_EXTENSIONS
#include <emscripten.h>
#endif

namespace duckdb {

//===--------------------------------------------------------------------===//
// Load External Extension
//===--------------------------------------------------------------------===//
#ifndef DUCKDB_DISABLE_EXTENSION_LOAD
typedef void (*ext_init_fun_t)(DatabaseInstance &);
typedef const char *(*ext_version_fun_t)(void);
typedef bool (*ext_is_storage_t)(void);

template <class T>
static T LoadFunctionFromDLL(void *dll, const string &function_name, const string &filename) {
	auto function = dlsym(dll, function_name.c_str());
	if (!function) {
		throw IOException("File \"%s\" did not contain function \"%s\": %s", filename, function_name, GetDLError());
	}
	return (T)function;
}

static void ComputeSHA256String(const std::string &to_hash, std::string *res) {
	// Invoke MbedTls function to actually compute sha256
	*res = duckdb_mbedtls::MbedTlsWrapper::ComputeSha256Hash(to_hash);
}

static void ComputeSHA256FileSegment(FileHandle *handle, const idx_t start, const idx_t end, std::string *res) {
	idx_t iter = start;
	const idx_t segment_size = 1024ULL * 8ULL;

	duckdb_mbedtls::MbedTlsWrapper::SHA256State state;

	std::string to_hash;
	while (iter < end) {
		idx_t len = std::min(end - iter, segment_size);
		to_hash.resize(len);
		handle->Read((void *)to_hash.data(), len, iter);

		state.AddString(to_hash);

		iter += segment_size;
	}

	*res = state.Finalize();
}
#endif

static string FilterZeroAtEnd(string s) {
	while (!s.empty() && s.back() == '\0') {
		s.pop_back();
	}
	return s;
}

static string PrettyPrintString(const string &s) {
	string res = "";
	for (auto c : s) {
		if (StringUtil::CharacterIsAlpha(c) || StringUtil::CharacterIsDigit(c) || c == '_' || c == '-' || c == ' ' ||
		    c == '.') {
			res += c;
		} else {
			uint8_t value = c;
			res += "\\x";
			uint8_t first = value / 16;
			if (first < 10) {
				res.push_back((char)('0' + first));
			} else {
				res.push_back((char)('a' + first - 10));
			}
			uint8_t second = value % 16;
			if (second < 10) {
				res.push_back((char)('0' + second));
			} else {
				res.push_back((char)('a' + second - 10));
			}
		}
	}
	return res;
}

bool ExtensionHelper::TryInitialLoad(DBConfig &config, FileSystem &fs, const string &extension,
                                     ExtensionInitResult &result, string &error) {
#ifdef DUCKDB_DISABLE_EXTENSION_LOAD
	throw PermissionException("Loading external extensions is disabled through a compile time flag");
#else
	if (!config.options.enable_external_access) {
		throw PermissionException("Loading external extensions is disabled through configuration");
	}
	auto filename = fs.ConvertSeparators(extension);

	// shorthand case
	if (!ExtensionHelper::IsFullPath(extension)) {
		string extension_name = ApplyExtensionAlias(extension);
#ifdef WASM_LOADABLE_EXTENSIONS
		string url_template = ExtensionUrlTemplate(&config, "");
		string url = ExtensionFinalizeUrlTemplate(url_template, extension_name);

		char *str = (char *)EM_ASM_PTR(
		    {
			    var jsString = ((typeof runtime == 'object') && runtime && (typeof runtime.whereToLoad == 'function') &&
			                    runtime.whereToLoad)
			                       ? runtime.whereToLoad(UTF8ToString($0))
			                       : (UTF8ToString($1));
			    var lengthBytes = lengthBytesUTF8(jsString) + 1;
			    // 'jsString.length' would return the length of the string as UTF-16
			    // units, but Emscripten C strings operate as UTF-8.
			    var stringOnWasmHeap = _malloc(lengthBytes);
			    stringToUTF8(jsString, stringOnWasmHeap, lengthBytes);
			    return stringOnWasmHeap;
		    },
		    filename.c_str(), url.c_str());
		std::string address(str);
		free(str);

		filename = address;
#else

		string local_path = !config.options.extension_directory.empty() ? config.options.extension_directory
		                                                                : ExtensionHelper::DefaultExtensionFolder(fs);

		// convert random separators to platform-canonic
		local_path = fs.ConvertSeparators(local_path);
		// expand ~ in extension directory
		local_path = fs.ExpandPath(local_path);
		auto path_components = PathComponents();
		for (auto &path_ele : path_components) {
			local_path = fs.JoinPath(local_path, path_ele);
		}
		filename = fs.JoinPath(local_path, extension_name + ".duckdb_extension");
#endif
	} else {
		filename = fs.ExpandPath(filename);
	}
#ifndef WASM_LOADABLE_EXTENSIONS
	if (!fs.FileExists(filename)) {
		string message;
		bool exact_match = ExtensionHelper::CreateSuggestions(extension, message);
		if (exact_match) {
			message += "\nInstall it first using \"INSTALL " + extension + "\".";
		}
		error = StringUtil::Format("Extension \"%s\" not found.\n%s", filename, message);
		return false;
	}
#endif

#ifdef WASM_LOADABLE_EXTENSIONS
	auto basename = fs.ExtractBaseName(filename);
	char *exe = NULL;
	exe = (char *)EM_ASM_PTR(
	    {
		    // Next few lines should argubly in separate JavaScript-land function call
		    // TODO: move them out / have them configurable

var url =(UTF8ToString($0));

    if (typeof XMLHttpRequest === "undefined" ) {
        const os = require('os');
        const path = require('path');
        const fs = require('fs');

        var array = url.split("/");
        var l = array.length;

        var folder = path.join(os.homedir(), ".duckdb/extensions/" + array[l - 4] + "/" + array[l - 3] + "/" + array[l - 2] + "/");
        var filePath = path.join(folder, array[l - 1]);

        try {
            if (!fs.existsSync(folder)) {
                fs.mkdirSync(folder, {
                    recursive: true
                });
            }

            if (!fs.existsSync(filePath)) {
                const int32 = new Int32Array(new SharedArrayBuffer(8));
var Worker = require('node:worker_threads').Worker;
                var worker = new Worker("const {Worker,isMainThread,parentPort,workerData,} = require('node:worker_threads');var times = 0;var SAB = 23;var Z = 0;async function ZZZ(e) {var x = await fetch(e);var res = await x.arrayBuffer();Atomics.store(SAB, 1, res.byteLength);Atomics.store(SAB, 0, 1);Atomics.notify(SAB, 1);Atomics.notify(SAB, 0);Z = res;};parentPort.on('message', function(event) {if (times == 0) {times++;SAB = event;} else if (times == 1) {times++;ZZZ(event);} else {const a = new Uint8Array(Z);const b = new Uint8Array(event.buffer);var K = Z.byteLength;for (var i = 0; i < K; i++) {b[i] = a[i];}Atomics.notify(event, 0);Atomics.store(SAB, 0, 2);Atomics.notify(SAB, 0);}});", {
                    eval: true
                });
                var uInt8Array;

                int32[0] = 0;
                int32[2] = 4;
                worker.postMessage(int32);

                worker.postMessage(url);
                Atomics.wait(int32, 0, 0);

                const int32_2 = new Int32Array(new SharedArrayBuffer(int32[1] + 3 - ((int32[1] + 3) % 4)));
                worker.postMessage(int32_2);

                Atomics.wait(int32, 0, 1);

                var x = new Uint8Array(int32_2.buffer, 0, int32[1]);
                uInt8Array = x;
                worker.terminate();
                fs.writeFileSync(filePath, uInt8Array);

            } else {
                uInt8Array = fs.readFileSync(filePath);
            }
        } catch (e) {
            console.log("Error fetching module", e);
            return 0;
        }
    } else {
        const xhr = new XMLHttpRequest();
        xhr.open("GET", url, false);
        xhr.responseType = "arraybuffer";
        xhr.send(null);
        if (xhr.status != 200)
            return 0;
        uInt8Array = xhr.response;
    }

		    var valid = WebAssembly.validate(uInt8Array);
	    var len = uInt8Array.byteLength;
		    var fileOnWasmHeap = _malloc(len + 4);

		var properArray = new Uint8Array(uInt8Array);

		    for (var iii = 0; iii < len; iii++) {
			    Module.HEAPU8[iii + fileOnWasmHeap + 4] = properArray[iii];
		    }
		    var LEN123 = new Uint8Array(4);
		    LEN123[0] = len % 256;
		    len -= LEN123[0];
		    len /= 256;
		    LEN123[1] = len % 256;
		    len -= LEN123[1];
		    len /= 256;
		    LEN123[2] = len % 256;
		    len -= LEN123[2];
		    len /= 256;
		    LEN123[3] = len % 256;
		    len -= LEN123[3];
		    len /= 256;
		    Module.HEAPU8.set(LEN123, fileOnWasmHeap);
               //FIXME: found how to expose those to the logger interface
		//console.log(LEN123);
		//console.log(properArray);
		//console.log(new Uint8Array(Module.HEAPU8, fileOnWasmHeap, len+4));
		// console.log('Loading extension ', UTF8ToString($1));

		    // Here we add the uInt8Array to Emscripten's filesystem, for it to be found by dlopen
		    FS.writeFile(UTF8ToString($1), new Uint8Array(uInt8Array));
		    return fileOnWasmHeap;
	    },
	    filename.c_str(), basename.c_str());
	if (!exe) {
		throw IOException("Extension %s is not available", filename);
	}

	auto dopen_from = basename;
	if (!config.options.allow_unsigned_extensions) {
		// signature is the last 256 bytes of the file

		string signature;
		signature.resize(256);

		D_ASSERT(exe);
		uint64_t LEN = 0;
		LEN *= 256;
		LEN += ((uint8_t *)exe)[3];
		LEN *= 256;
		LEN += ((uint8_t *)exe)[2];
		LEN *= 256;
		LEN += ((uint8_t *)exe)[1];
		LEN *= 256;
	LEN += ((uint8_t *)exe)[0];
		auto signature_offset = LEN - signature.size();

		const idx_t maxLenChunks = 1024ULL * 1024ULL;
		const idx_t numChunks = (signature_offset + maxLenChunks - 1) / maxLenChunks;
		std::vector<std::string> hash_chunks(numChunks);
		std::vector<idx_t> splits(numChunks + 1);

		for (idx_t i = 0; i < numChunks; i++) {
			splits[i] = maxLenChunks * i;
		}
		splits.back() = signature_offset;

		for (idx_t i = 0; i < numChunks; i++) {
			string x;
			x.resize(splits[i + 1] - splits[i]);
			for (idx_t j = 0; j < x.size(); j++) {
				x[j] = ((uint8_t *)exe)[j + 4 + splits[i]];
			}
			ComputeSHA256String(x, &hash_chunks[i]);
		}

		string hash_concatenation;
		hash_concatenation.reserve(32 * numChunks); // 256 bits -> 32 bytes per chunk

		for (auto &hash_chunk : hash_chunks) {
			hash_concatenation += hash_chunk;
		}

		string two_level_hash;
		ComputeSHA256String(hash_concatenation, &two_level_hash);

		for (idx_t j = 0; j < signature.size(); j++) {
			signature[j] = ((uint8_t *)exe)[4 + signature_offset + j];
		}
		bool any_valid = false;
		for (auto &key : ExtensionHelper::GetPublicKeys()) {
			if (duckdb_mbedtls::MbedTlsWrapper::IsValidSha256Signature(key, signature, two_level_hash)) {
				any_valid = true;
				break;
			}
		}
		if (!any_valid) {
			throw IOException(config.error_manager->FormatException(ErrorType::UNSIGNED_EXTENSION, filename));
		}
	}
	if (exe) {
		free(exe);
	}
std::string extension_version="";
#else
	string metadata_segment;
	metadata_segment.resize(512);

	const std::string engine_version = std::string(GetVersionDirectoryName());
	const std::string engine_platform = std::string(DuckDB::Platform());

	auto handle = fs.OpenFile(filename, FileFlags::FILE_FLAGS_READ);

	idx_t file_size = handle->GetFileSize();

	if (file_size < 1024) {
		throw InvalidInputException(
		    "Extension \"%s\" do not have metadata compatible with DuckDB loading it "
		    "(version %s, platform %s). File size in particular is lower than minimum threshold of 1024",
		    filename, engine_version, engine_platform);
	}

	auto metadata_offset = file_size - metadata_segment.size();

	handle->Read((void *)metadata_segment.data(), metadata_segment.size(), metadata_offset);

	std::vector<std::string> metadata_field;
	for (idx_t i = 0; i < 8; i++) {
		metadata_field.emplace_back(metadata_segment, i * 32, 32);
	}

	std::reverse(metadata_field.begin(), metadata_field.end());

	std::string extension_duckdb_platform = FilterZeroAtEnd(metadata_field[1]);
	std::string extension_duckdb_version = FilterZeroAtEnd(metadata_field[2]);
	std::string extension_version = FilterZeroAtEnd(metadata_field[3]);

	string metadata_mismatch_error = "";
	{
		char a[32] = {0};
		a[0] = '4';
		if (strncmp(a, metadata_field[0].data(), 32) != 0) {
			// metadata do not looks right, add this to the error message
			metadata_mismatch_error =
			    "\n" + StringUtil::Format("Extension \"%s\" do not have metadata compatible with DuckDB "
			                              "loading it (version %s, platform %s)",
			                              filename, engine_version, engine_platform);
		} else if (engine_version != extension_duckdb_version || engine_platform != extension_duckdb_platform) {
			metadata_mismatch_error = "\n" + StringUtil::Format("Extension \"%s\" (version %s, platfrom %s) does not "
			                                                    "match DuckDB loading it (version %s, platform %s)",
			                                                    filename, PrettyPrintString(extension_duckdb_version),
			                                                    PrettyPrintString(extension_duckdb_platform),
			                                                    engine_version, engine_platform);

		} else {
			// All looks good
		}
	}

	if (!config.options.allow_unsigned_extensions) {
		// signature is the last 256 bytes of the file
		string signature(metadata_segment, metadata_segment.size() - 256);

		auto signature_offset = metadata_offset + metadata_segment.size() - signature.size();

		const idx_t maxLenChunks = 1024ULL * 1024ULL;
		const idx_t numChunks = (signature_offset + maxLenChunks - 1) / maxLenChunks;
		std::vector<std::string> hash_chunks(numChunks);
		std::vector<idx_t> splits(numChunks + 1);

		for (idx_t i = 0; i < numChunks; i++) {
			splits[i] = maxLenChunks * i;
		}
		splits.back() = signature_offset;

#ifndef DUCKDB_NO_THREADS
		std::vector<std::thread> threads;
		threads.reserve(numChunks);
		for (idx_t i = 0; i < numChunks; i++) {
			threads.emplace_back(ComputeSHA256FileSegment, handle.get(), splits[i], splits[i + 1], &hash_chunks[i]);
		}

		for (auto &thread : threads) {
			thread.join();
		}
#else
		for (idx_t i = 0; i < numChunks; i++) {
			ComputeSHA256FileSegment(handle.get(), splits[i], splits[i + 1], &hash_chunks[i]);
		}
#endif // DUCKDB_NO_THREADS

		string hash_concatenation;
		hash_concatenation.reserve(32 * numChunks); // 256 bits -> 32 bytes per chunk

		for (auto &hash_chunk : hash_chunks) {
			hash_concatenation += hash_chunk;
		}

		string two_level_hash;
		ComputeSHA256String(hash_concatenation, &two_level_hash);

		// TODO maybe we should do a stream read / hash update here
		handle->Read((void *)signature.data(), signature.size(), signature_offset);

		bool any_valid = false;
		for (auto &key : ExtensionHelper::GetPublicKeys()) {
			if (duckdb_mbedtls::MbedTlsWrapper::IsValidSha256Signature(key, signature, two_level_hash)) {
				any_valid = true;
				break;
			}
		}
		if (!any_valid) {
			throw IOException(config.error_manager->FormatException(ErrorType::UNSIGNED_EXTENSION, filename) +
			                  metadata_mismatch_error);
		}

		if (!metadata_mismatch_error.empty()) {
			// Signed extensions perform the full check
			throw InvalidInputException(metadata_mismatch_error.substr(1));
		}
	} else if (!config.options.allow_extensions_metadata_mismatch) {
		if (!metadata_mismatch_error.empty()) {
			// Unsigned extensions AND configuration allowing metadata_mismatch_error, loading allowed, mainly for
			// debugging purposes
			throw InvalidInputException(metadata_mismatch_error.substr(1));
		}
	}

	auto number_metadata_fields = 3;
	D_ASSERT(number_metadata_fields == 3); // Currently hardcoded value
	metadata_field.resize(number_metadata_fields + 1);
#endif
	auto filebase = fs.ExtractBaseName(filename);

#ifdef WASM_LOADABLE_EXTENSIONS
#else
	auto dopen_from = filename;
#endif

	auto lib_hdl = dlopen(dopen_from.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!lib_hdl) {
		throw IOException("Extension \"%s\" could not be loaded: %s", filename, GetDLError());
	}

	auto lowercase_extension_name = StringUtil::Lower(filebase);

	result.filebase = lowercase_extension_name;
	result.extension_version = extension_version;
	result.filename = filename;
	result.lib_hdl = lib_hdl;
	return true;
#endif
}

ExtensionInitResult ExtensionHelper::InitialLoad(DBConfig &config, FileSystem &fs, const string &extension) {
	string error;
	ExtensionInitResult result;
	if (!TryInitialLoad(config, fs, extension, result, error)) {
		if (!ExtensionHelper::AllowAutoInstall(extension)) {
			throw IOException(error);
		}
		// the extension load failed - try installing the extension
		ExtensionHelper::InstallExtension(config, fs, extension, false);
		// try loading again
		if (!TryInitialLoad(config, fs, extension, result, error)) {
			throw IOException(error);
		}
	}
	return result;
}

bool ExtensionHelper::IsFullPath(const string &extension) {
	return StringUtil::Contains(extension, ".") || StringUtil::Contains(extension, "/") ||
	       StringUtil::Contains(extension, "\\");
}

string ExtensionHelper::GetExtensionName(const string &original_name) {
	auto extension = StringUtil::Lower(original_name);
	if (!IsFullPath(extension)) {
		return ExtensionHelper::ApplyExtensionAlias(extension);
	}
	auto splits = StringUtil::Split(StringUtil::Replace(extension, "\\", "/"), '/');
	if (splits.empty()) {
		return ExtensionHelper::ApplyExtensionAlias(extension);
	}
	splits = StringUtil::Split(splits.back(), '.');
	if (splits.empty()) {
		return ExtensionHelper::ApplyExtensionAlias(extension);
	}
	return ExtensionHelper::ApplyExtensionAlias(splits.front());
}

void ExtensionHelper::LoadExternalExtension(DatabaseInstance &db, FileSystem &fs, const string &extension) {
	if (db.ExtensionIsLoaded(extension)) {
		return;
	}
#ifdef DUCKDB_DISABLE_EXTENSION_LOAD
	throw PermissionException("Loading external extensions is disabled through a compile time flag");
#else
	auto res = InitialLoad(DBConfig::GetConfig(db), fs, extension);
	auto init_fun_name = res.filebase + "_init";

	ext_init_fun_t init_fun;
	init_fun = LoadFunctionFromDLL<ext_init_fun_t>(res.lib_hdl, init_fun_name, res.filename);

	try {
		(*init_fun)(db);
	} catch (std::exception &e) {
		ErrorData error(e);
		throw InvalidInputException("Initialization function \"%s\" from file \"%s\" threw an exception: \"%s\"",
		                            init_fun_name, res.filename, error.RawMessage());
	}

	db.SetExtensionLoaded(extension, res.extension_version);
#endif
}

void ExtensionHelper::LoadExternalExtension(ClientContext &context, const string &extension) {
	LoadExternalExtension(DatabaseInstance::GetDatabase(context), FileSystem::GetFileSystem(context), extension);
}

string ExtensionHelper::ExtractExtensionPrefixFromPath(const string &path) {
	auto first_colon = path.find(':');
	if (first_colon == string::npos || first_colon < 2) { // needs to be at least two characters because windows c: ...
		return "";
	}
	auto extension = path.substr(0, first_colon);

	if (path.substr(first_colon, 3) == "://") {
		// these are not extensions
		return "";
	}

	D_ASSERT(extension.size() > 1);
	// needs to be alphanumeric
	for (auto &ch : extension) {
		if (!isalnum(ch) && ch != '_') {
			return "";
		}
	}
	return extension;
}

} // namespace duckdb
