#include "duckdb/common/extended_file_info_file_system.hpp"

#include "duckdb/common/algorithm.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/multi_file/multi_file_list.hpp"
#include "duckdb/common/string_util.hpp"

#include <cstring>

namespace duckdb {

//===--------------------------------------------------------------------===//
// File system
//===--------------------------------------------------------------------===//
std::string ExtendedFileInfoFileSystem::GetName() const {
	return NAME;
}

bool ExtendedFileInfoFileSystem::CanHandleFile(const string &fpath) {
	return IsEncodedPath(fpath);
}

bool ExtendedFileInfoFileSystem::IsRewritingFileSystem() const {
	return true;
}

OpenFileInfo ExtendedFileInfoFileSystem::RewriteFile(const OpenFileInfo &file) {
	return Decode(file);
}

unique_ptr<MultiFileList> ExtendedFileInfoFileSystem::GlobFilesExtended(const string &path, const FileGlobInput &input,
                                                                        optional_ptr<FileOpener> opener) {
	vector<OpenFileInfo> files;
	files.push_back(Decode(path));
	return make_uniq<SimpleMultiFileList>(std::move(files));
}

//===--------------------------------------------------------------------===//
// Encoded paths
//===--------------------------------------------------------------------===//
namespace {

const char OPTIONS_OPEN = '{';
const char OPTIONS_CLOSE = '}';
const char OPTION_SEPARATOR = ';';
const char OPTION_ASSIGN = '=';

//! Decode a (URL-encoded) component of the options block
string DecodeComponent(const string &encoded_path, const string &component) {
	try {
		return StringUtil::URLDecode(component);
	} catch (std::exception &ex) {
		throw InvalidInputException("Malformed file info path \"%s\": %s", encoded_path, ex.what());
	}
}

//! The options of a file ordered by name, so the encoding of a file is stable
vector<pair<string, Value>> SortedOptions(const ExtendedOpenFileInfo &extended_info) {
	vector<pair<string, Value>> options(extended_info.options.begin(), extended_info.options.end());
	std::sort(options.begin(), options.end(),
	          [](const pair<string, Value> &a, const pair<string, Value> &b) { return a.first < b.first; });
	return options;
}

} // namespace

bool ExtendedFileInfoFileSystem::IsEncodedPath(const string &path) {
	return StringUtil::StartsWith(path, ENCODED_PATH_PREFIX);
}

string ExtendedFileInfoFileSystem::EncodePath(const OpenFileInfo &file) {
	auto &path = file.path;
	auto &extended_info = file.extended_info;
	if (!extended_info) {
		return path;
	}
	if (IsEncodedPath(path)) {
		throw InvalidInputException("Cannot pack file info path \"%s\": the path is already a file info path", path);
	}
	auto options = SortedOptions(*extended_info);

	string result = ENCODED_PATH_PREFIX;
	result += OPTIONS_OPEN;
	bool first = true;
	for (auto &option : options) {
		if (option.second.IsNull()) {
			// a NULL option is an option that was not specified
			continue;
		}
		if (!first) {
			result += OPTION_SEPARATOR;
		}
		first = false;
		// options are written as text - it is the type an option is read back as that matters, not the type it
		// was written as
		result += StringUtil::URLEncode(option.first);
		result += OPTION_ASSIGN;
		result += StringUtil::URLEncode(option.second.ToString());
	}
	result += OPTIONS_CLOSE;
	result += path;
	return result;
}

bool ExtendedFileInfoFileSystem::TryDecodePath(const string &path, OpenFileInfo &result) {
	if (!IsEncodedPath(path)) {
		return false;
	}
	auto prefix_length = strlen(ENCODED_PATH_PREFIX);
	if (path.size() <= prefix_length || path[prefix_length] != OPTIONS_OPEN) {
		throw InvalidInputException("Malformed file info path \"%s\": expected \"%s%c\" followed by the options", path,
		                            ENCODED_PATH_PREFIX, OPTIONS_OPEN);
	}
	auto options_end = path.find(OPTIONS_CLOSE, prefix_length);
	if (options_end == string::npos) {
		throw InvalidInputException("Malformed file info path \"%s\": the options are not closed with \"%c\"", path,
		                            OPTIONS_CLOSE);
	}
	auto options_block = path.substr(prefix_length + 1, options_end - prefix_length - 1);
	result.path = path.substr(options_end + 1);
	if (result.path.empty()) {
		throw InvalidInputException("Malformed file info path \"%s\": no path follows the options", path);
	}
	if (IsEncodedPath(result.path)) {
		throw InvalidInputException("Malformed file info path \"%s\": a file info path cannot hold another file "
		                            "info path",
		                            path);
	}
	auto extended_info = make_shared_ptr<ExtendedOpenFileInfo>();
	if (!options_block.empty()) {
		for (auto &option : StringUtil::Split(options_block, OPTION_SEPARATOR)) {
			auto assign_pos = option.find(OPTION_ASSIGN);
			if (assign_pos == string::npos) {
				throw InvalidInputException("Malformed file info path \"%s\": option \"%s\" has no value", path,
				                            option);
			}
			auto key = DecodeComponent(path, option.substr(0, assign_pos));
			if (key.empty()) {
				throw InvalidInputException("Malformed file info path \"%s\": option \"%s\" has no name", path, option);
			}
			extended_info->options[key] = Value(DecodeComponent(path, option.substr(assign_pos + 1)));
		}
	}
	result.extended_info = std::move(extended_info);
	return true;
}

OpenFileInfo ExtendedFileInfoFileSystem::Decode(const string &path) {
	OpenFileInfo result;
	if (!TryDecodePath(path, result)) {
		result.path = path;
	}
	return result;
}

OpenFileInfo ExtendedFileInfoFileSystem::Decode(const OpenFileInfo &file) {
	OpenFileInfo result;
	if (!TryDecodePath(file.path, result)) {
		return file;
	}
	if (file.extended_info) {
		// options set on the file itself are the more deliberate ones - they win over the encoded options
		for (auto &entry : file.extended_info->options) {
			result.extended_info->options[entry.first] = entry.second;
		}
	}
	return result;
}

Value ExtendedFileInfoFileSystem::ToValue(const OpenFileInfo &file) {
	child_list_t<Value> children;
	children.emplace_back("filename", Value(file.path));
	if (file.extended_info) {
		for (auto &option : SortedOptions(*file.extended_info)) {
			children.emplace_back(option.first, option.second);
		}
	}
	return Value::STRUCT(std::move(children));
}

} // namespace duckdb
