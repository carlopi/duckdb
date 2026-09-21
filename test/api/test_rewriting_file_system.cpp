#include "catch.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/local_file_system.hpp"
#include "duckdb/common/multi_file/multi_file_list.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/virtual_file_system.hpp"
#include "duckdb/main/database.hpp"
#include "test_helpers.hpp"

using namespace duckdb;

namespace {

//! Rewrites "<scheme>://<path>" into "<target><path>", attaching an open option that records the rewrite
class AliasFileSystem : public FileSystem {
public:
	AliasFileSystem(string name_p, string scheme_p, string target_p)
	    : name(std::move(name_p)), scheme(std::move(scheme_p)), target(std::move(target_p)) {
	}

	string GetName() const override {
		return name;
	}
	bool CanHandleFile(const string &path) override {
		return StringUtil::StartsWith(path, scheme);
	}
	bool IsRewritingFileSystem() const override {
		return true;
	}
	OpenFileInfo RewriteFile(const OpenFileInfo &file) override {
		rewrites++;
		OpenFileInfo result(target + file.path.substr(scheme.size()));
		result.extended_info = make_shared_ptr<ExtendedOpenFileInfo>();
		if (file.extended_info) {
			result.extended_info->options = file.extended_info->options;
		}
		result.extended_info->options["rewritten_by"] = Value(name);
		return result;
	}

	//! A rewriting file system decides what globbing its paths means - here a path names exactly one file
	unique_ptr<MultiFileList> GlobFilesExtended(const string &path, const FileGlobInput &input,
	                                            optional_ptr<FileOpener> opener) override {
		vector<OpenFileInfo> files;
		files.push_back(RewriteFile(OpenFileInfo(path)));
		return make_uniq<SimpleMultiFileList>(std::move(files));
	}
	bool SupportsGlobExtended() const override {
		return true;
	}

	idx_t rewrites = 0;

private:
	string name;
	string scheme;
	string target;
};

//! A local file system that records the files it was asked to open
class RecordingLocalFileSystem : public LocalFileSystem {
public:
	string GetName() const override {
		return "LocalFileSystem";
	}

	vector<OpenFileInfo> opened;

protected:
	unique_ptr<FileHandle> OpenFileExtended(const OpenFileInfo &file, FileOpenFlags flags,
	                                        optional_ptr<FileOpener> opener) override {
		opened.push_back(file);
		return LocalFileSystem::OpenFile(file.path, flags, opener);
	}
	bool SupportsOpenFileExtended() const override {
		return true;
	}
};

} // namespace

TEST_CASE("Rewriting file systems are resolved by the virtual file system", "[api][.]") {
	auto csv_path = TestCreatePath("rewriting_fs.csv");
	auto dir = TestCreatePath("") + "/";
	{
		LocalFileSystem local;
		auto handle = local.OpenFile(csv_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		string content = "a,b\n42,hello\n";
		handle->Write(QueryContext(), const_cast<char *>(content.c_str()), content.size(), 0);
	}

	DBConfig config;
	auto recording_fs = make_uniq<RecordingLocalFileSystem>();
	auto &recording = *recording_fs;
	config.file_system = make_uniq<VirtualFileSystem>(std::move(recording_fs));
	DuckDB db(nullptr, &config);
	Connection con(db);
	auto &vfs = db.instance->GetFileSystem();

	auto alias_fs = make_uniq<AliasFileSystem>("AliasFileSystem", "alias://", dir);
	auto &alias = *alias_fs;
	vfs.RegisterSubSystem(std::move(alias_fs));

	SECTION("a rewritten path is opened on the file system it rewrites to, with the rewritten options") {
		auto result = con.Query("SELECT * FROM read_csv('alias://rewriting_fs.csv')");
		REQUIRE_NO_FAIL(*result);
		REQUIRE(CHECK_COLUMN(result, 0, {42}));
		REQUIRE(CHECK_COLUMN(result, 1, {"hello"}));
		REQUIRE(alias.rewrites > 0);

		// every open sees the rewritten path - the options travel with the opens that pass an OpenFileInfo
		REQUIRE(!recording.opened.empty());
		for (auto &file : recording.opened) {
			REQUIRE(file.path == csv_path);
			if (!file.extended_info) {
				continue;
			}
			string rewritten_by;
			REQUIRE(file.extended_info->TryGetOption("rewritten_by", rewritten_by));
			REQUIRE(rewritten_by == "AliasFileSystem");
		}
	}

	SECTION("the path-only methods resolve as well") {
		REQUIRE(vfs.FileExists("alias://rewriting_fs.csv"));
		REQUIRE(!vfs.FileExists("alias://does_not_exist.csv"));
		REQUIRE(vfs.FileExists(csv_path));
	}

	SECTION("options set on the file itself are handed to the rewriting file system") {
		OpenFileInfo file("alias://rewriting_fs.csv");
		file.extended_info = make_shared_ptr<ExtendedOpenFileInfo>();
		file.extended_info->options["my_option"] = Value::INTEGER(42);
		auto handle = vfs.OpenFile(file, FileFlags::FILE_FLAGS_READ);
		REQUIRE(handle);
		REQUIRE(handle->GetPath() == csv_path);
		auto &opened = recording.opened.back();
		REQUIRE(opened.path == csv_path);
		REQUIRE(opened.extended_info);
		idx_t my_option;
		REQUIRE(opened.extended_info->TryGetOption("my_option", my_option));
		REQUIRE(my_option == 42);
		string rewritten_by;
		REQUIRE(opened.extended_info->TryGetOption("rewritten_by", rewritten_by));
		REQUIRE(rewritten_by == "AliasFileSystem");
	}

	SECTION("globbing a rewritten path is up to the rewriting file system") {
		auto result = con.Query("SELECT file FROM glob('alias://rewriting_fs.csv')");
		REQUIRE_NO_FAIL(*result);
		REQUIRE(CHECK_COLUMN(result, 0, {Value(csv_path)}));
		// the file reported by glob carries the rewritten path, so the filename column shows the real file
		result = con.Query("SELECT filename FROM read_csv('alias://rewriting_fs.csv', filename=true)");
		REQUIRE_NO_FAIL(*result);
		REQUIRE(CHECK_COLUMN(result, 0, {Value(csv_path)}));
	}

	SECTION("a path is rewritten at most once") {
		vfs.RegisterSubSystem(make_uniq<AliasFileSystem>("ChainedAliasFileSystem", "chained://", "alias://"));
		REQUIRE_THROWS_WITH(vfs.OpenFile("chained://rewriting_fs.csv", FileFlags::FILE_FLAGS_READ),
		                    Catch::Contains("can be rewritten only once"));
		REQUIRE_THROWS_WITH(vfs.FileExists("chained://rewriting_fs.csv"),
		                    Catch::Contains("can be rewritten only once"));
	}

	SECTION("a rewriting file system can be disabled like any other") {
		REQUIRE_NO_FAIL(con.Query("SET disabled_filesystems='AliasFileSystem'"));
		auto result = con.Query("SELECT * FROM read_csv('alias://rewriting_fs.csv')");
		REQUIRE_FAIL(result);
		REQUIRE(StringUtil::Contains(result->GetError(), "AliasFileSystem has been disabled"));
		REQUIRE(vfs.IsDisabledForPath("alias://rewriting_fs.csv"));
		REQUIRE(!vfs.IsDisabledForPath(csv_path));
		REQUIRE_NO_FAIL(con.Query("SELECT * FROM read_csv('" + csv_path + "')"));
	}

	SECTION("disabling the file system a path rewrites to disables the rewritten path") {
		REQUIRE_NO_FAIL(con.Query("SET disabled_filesystems='LocalFileSystem'"));
		REQUIRE(vfs.IsDisabledForPath("alias://rewriting_fs.csv"));
		auto result = con.Query("SELECT * FROM read_csv('alias://rewriting_fs.csv')");
		REQUIRE_FAIL(result);
		REQUIRE(StringUtil::Contains(result->GetError(), "LocalFileSystem has been disabled"));
	}
}
