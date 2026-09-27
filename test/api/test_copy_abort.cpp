#include "catch.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/local_file_system.hpp"
#include "duckdb/common/serializer/async_file_writer.hpp"
#include "duckdb/common/serializer/async_memory_governor.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types/column/column_data_collection.hpp"
#include "duckdb/execution/operator/persistent/copy_output_state.hpp"
#include "duckdb/function/copy_function.hpp"
#include "duckdb/function/copy_function_output.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/main/extension_manager.hpp"
#include "duckdb/parallel/task_executor.hpp"
#include "test_helpers.hpp"

#include <condition_variable>
#include <thread>

using namespace duckdb;

namespace {

struct CopyAbortTestInfo : CopyFunctionInfo {
	CopyAbortTestInfo(bool fail_sink_p, bool fail_finalize_p = false, bool block_initialize_p = false,
	                  bool create_file_p = false)
	    : fail_sink(fail_sink_p), fail_finalize(fail_finalize_p), block_initialize(block_initialize_p),
	      create_file(create_file_p) {
	}

	bool fail_sink;
	bool fail_finalize;
	bool block_initialize;
	bool create_file;
	bool fail_initialize = false;
	bool fail_before_open = false;
	bool fail_after_open = false;
	bool skip_open = false;
	bool double_open = false;
	bool return_empty_state = false;
	bool close_in_finalize = false;
	atomic<idx_t> initialize_count {0};
	atomic<idx_t> finalize_count {0};
	atomic<idx_t> destroy_count {0};
	atomic<idx_t> flush_count {0};
	atomic<bool> release_initialize {false};
	shared_ptr<string> managed_write_data;
};

struct CopyAbortBindData : FunctionData {
	explicit CopyAbortBindData(shared_ptr<CopyFunctionInfo> info_p) : info(std::move(info_p)) {
	}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<CopyAbortBindData>(info);
	}

	bool Equals(const FunctionData &other_p) const override {
		auto &other = other_p.Cast<CopyAbortBindData>();
		return info == other.info;
	}

	CopyAbortTestInfo &GetInfo() {
		return info->Cast<CopyAbortTestInfo>();
	}

	shared_ptr<CopyFunctionInfo> info;
};

struct CopyAbortLocalData : LocalFunctionData {};

struct CopyAbortGlobalData : GlobalFunctionData {
	explicit CopyAbortGlobalData(unique_ptr<FileHandle> handle_p) : handle(std::move(handle_p)) {
	}

	unique_ptr<FileHandle> handle;
};

struct CopyAbortPreparedData : PreparedBatchData {};

class CopyManagedRaceFileSystem : public LocalFileSystem {
public:
	explicit CopyManagedRaceFileSystem(string actual_path_p) : actual_path(std::move(actual_path_p)) {
	}

	string GetName() const override {
		return "CopyManagedRaceFileSystem";
	}

	bool CanHandleFile(const string &path) override {
		return StringUtil::StartsWith(path, "copy-managed-race://");
	}

	bool FileExists(const string &path, optional_ptr<FileOpener> opener) override {
		if (!CanHandleFile(path)) {
			return LocalFileSystem::FileExists(path, opener);
		}
		if (!created_competing_file.exchange(true)) {
			auto handle = LocalFileSystem::OpenFile(
			    actual_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW, opener);
			handle->Close();
			return false;
		}
		return LocalFileSystem::FileExists(actual_path, opener);
	}

	unique_ptr<FileHandle> OpenFile(const string &path, FileOpenFlags flags, optional_ptr<FileOpener> opener) override {
		return LocalFileSystem::OpenFile(CanHandleFile(path) ? actual_path : path, flags, opener);
	}

private:
	const string actual_path;
	atomic<bool> created_competing_file {false};
};

class CopyManagedCloseOnlyFileSystem : public LocalFileSystem {
public:
	explicit CopyManagedCloseOnlyFileSystem(string actual_path_p) : actual_path(std::move(actual_path_p)) {
	}

	string GetName() const override {
		return "CopyManagedCloseOnlyFileSystem";
	}

	bool CanHandleFile(const string &path) override {
		return StringUtil::StartsWith(path, "copy-managed-close-only://");
	}

	bool FileExists(const string &path, optional_ptr<FileOpener> opener) override {
		return LocalFileSystem::FileExists(MapPath(path), opener);
	}

	unique_ptr<FileHandle> OpenFile(const string &path, FileOpenFlags flags, optional_ptr<FileOpener> opener) override {
		return LocalFileSystem::OpenFile(MapPath(path), flags, opener);
	}

	void RemoveFile(const string &path, optional_ptr<FileOpener> opener) override {
		LocalFileSystem::RemoveFile(MapPath(path), opener);
	}

	void AbortFileWrite(FileHandle &handle) override {
		FileSystem::AbortFileWrite(handle);
	}

private:
	const string &MapPath(const string &path) {
		return CanHandleFile(path) ? actual_path : path;
	}

private:
	const string actual_path;
};

class CopyAmbiguousPublishFileHandle : public FileHandle {
public:
	CopyAmbiguousPublishFileHandle(FileSystem &fs, const string &path, FileOpenFlags flags, atomic<bool> &published_p,
	                               atomic<idx_t> &publish_count_p)
	    : FileHandle(fs, path, flags), published(published_p), publish_count(publish_count_p) {
	}

	void Close() override {
		if (finished) {
			return;
		}
		finished = true;
		published = true;
		publish_count++;
		throw IOException("Injected ambiguous COPY publish failure");
	}

	void Abort() {
		finished = true;
	}

private:
	atomic<bool> &published;
	atomic<idx_t> &publish_count;
	bool finished = false;
};

class CopyAmbiguousPublishFileSystem : public LocalFileSystem {
public:
	string GetName() const override {
		return "CopyAmbiguousPublishFileSystem";
	}

	bool CanHandleFile(const string &path) override {
		return StringUtil::StartsWith(path, "copy-ambiguous-publish://");
	}

	unique_ptr<FileHandle> OpenFile(const string &path, FileOpenFlags flags,
	                                optional_ptr<FileOpener> = nullptr) override {
		return make_uniq<CopyAmbiguousPublishFileHandle>(*this, path, flags, published, publish_count);
	}

	bool IsLocalFileSystem() const override {
		return false;
	}

	bool OnDiskFile(FileHandle &) override {
		return false;
	}

	bool FileExists(const string &, optional_ptr<FileOpener> = nullptr) override {
		return published;
	}

	void RemoveFile(const string &, optional_ptr<FileOpener> = nullptr) override {
		published = false;
		remove_count++;
	}

	FileWriteMode GetWriteMode(FileHandle &) override {
		return FileWriteMode::SEQUENTIAL;
	}

	int64_t GetFileSize(FileHandle &) override {
		return UnsafeNumericCast<int64_t>(written_bytes.load());
	}

	FileMetadata Stats(FileHandle &) override {
		FileMetadata result;
		result.file_size = UnsafeNumericCast<int64_t>(written_bytes.load());
		result.file_type = FileType::FILE_TYPE_REGULAR;
		return result;
	}

	int64_t Write(FileHandle &, void *, int64_t nr_bytes) override {
		written_bytes += UnsafeNumericCast<idx_t>(nr_bytes);
		return nr_bytes;
	}

	void Write(FileHandle &, void *, int64_t, idx_t) override {
		throw NotImplementedException("Injected missing positional write support");
	}

	void AbortFileWrite(FileHandle &handle) override {
		abort_count++;
		handle.Cast<CopyAmbiguousPublishFileHandle>().Abort();
	}

public:
	atomic<bool> published {false};
	atomic<idx_t> publish_count {0};
	atomic<idx_t> remove_count {0};
	atomic<idx_t> abort_count {0};
	atomic<idx_t> written_bytes {0};
};

class CopyBackpressureFileSystem : public LocalFileSystem {
public:
	explicit CopyBackpressureFileSystem(string actual_path_p) : actual_path(std::move(actual_path_p)) {
	}

	string GetName() const override {
		return "CopyBackpressureFileSystem";
	}

	bool CanHandleFile(const string &path) override {
		return StringUtil::StartsWith(path, "copy-backpressure://");
	}

	bool IsLocalFileSystem() const override {
		return false;
	}

	bool OnDiskFile(FileHandle &) override {
		return false;
	}

	bool FileExists(const string &path, optional_ptr<FileOpener> opener) override {
		return LocalFileSystem::FileExists(MapPath(path), opener);
	}

	unique_ptr<FileHandle> OpenFile(const string &path, FileOpenFlags flags, optional_ptr<FileOpener> opener) override {
		return LocalFileSystem::OpenFile(MapPath(path), flags, opener);
	}

	void RemoveFile(const string &path, optional_ptr<FileOpener> opener) override {
		LocalFileSystem::RemoveFile(MapPath(path), opener);
	}

	FileWriteMode GetWriteMode(FileHandle &) override {
		return FileWriteMode::SEQUENTIAL;
	}

	void Write(FileHandle &, void *, int64_t, idx_t) override {
		throw NotImplementedException("Injected missing positional write support");
	}

	int64_t Write(FileHandle &, void *, int64_t nr_bytes) override {
		unique_lock<mutex> guard(lock);
		entered_writes++;
		cv.notify_all();
		cv.wait(guard, [&]() { return released; });
		write_count++;
		written_bytes += NumericCast<idx_t>(nr_bytes);
		return nr_bytes;
	}

	bool WaitForWrite() {
		unique_lock<mutex> guard(lock);
		return cv.wait_for(guard, std::chrono::seconds(5), [&]() { return entered_writes > 0; });
	}

	void ReleaseWrites() {
		{
			lock_guard<mutex> guard(lock);
			released = true;
		}
		cv.notify_all();
	}

	idx_t WriteCount() {
		lock_guard<mutex> guard(lock);
		return write_count;
	}

	idx_t WrittenBytes() {
		lock_guard<mutex> guard(lock);
		return written_bytes;
	}

private:
	const string &MapPath(const string &path) const {
		return StringUtil::StartsWith(path, "copy-backpressure://") ? actual_path : path;
	}

private:
	const string actual_path;
	mutex lock;
	std::condition_variable cv;
	idx_t entered_writes = 0;
	idx_t write_count = 0;
	idx_t written_bytes = 0;
	bool released = false;
};

class CopyBlockingAsyncTaskState {
public:
	bool WaitForStarted() {
		unique_lock<mutex> guard(lock);
		return cv.wait_for(guard, std::chrono::seconds(5), [&]() { return started; });
	}

	void Enter() {
		unique_lock<mutex> guard(lock);
		started = true;
		cv.notify_all();
		cv.wait(guard, [&]() { return released; });
	}

	void Release() {
		{
			lock_guard<mutex> guard(lock);
			released = true;
		}
		cv.notify_all();
	}

private:
	mutex lock;
	std::condition_variable cv;
	bool started = false;
	bool released = false;
};

class CopyBlockingAsyncTask : public BaseExecutorTask {
public:
	CopyBlockingAsyncTask(TaskExecutor &executor, CopyBlockingAsyncTaskState &state_p)
	    : BaseExecutorTask(executor), state(state_p) {
	}

	void ExecuteTask() override {
		state.Enter();
	}

private:
	CopyBlockingAsyncTaskState &state;
};

class CopyAsyncThreadBlocker {
public:
	explicit CopyAsyncThreadBlocker(ClientContext &context) : executor(context, TaskSchedulerType::ASYNC) {
		executor.ScheduleTask(make_uniq<CopyBlockingAsyncTask>(executor, state));
	}

	~CopyAsyncThreadBlocker() {
		Release();
	}

	bool WaitForStarted() {
		return state.WaitForStarted();
	}

	void Release() {
		if (released) {
			return;
		}
		state.Release();
		try {
			executor.WorkOnTasks();
		} catch (...) { // NOLINT
		}
		released = true;
	}

private:
	CopyBlockingAsyncTaskState state;
	TaskExecutor executor;
	bool released = false;
};

class CopyBackpressureRunGuard {
public:
	CopyBackpressureRunGuard(CopyBackpressureFileSystem &fs_p, CopyAsyncThreadBlocker &blocker_p,
	                         std::thread &query_thread_p)
	    : fs(fs_p), blocker(blocker_p), query_thread(query_thread_p) {
	}

	~CopyBackpressureRunGuard() {
		Release();
	}

	void Release() {
		if (released) {
			return;
		}
		fs.ReleaseWrites();
		blocker.Release();
		if (query_thread.joinable()) {
			query_thread.join();
		}
		released = true;
	}

private:
	CopyBackpressureFileSystem &fs;
	CopyAsyncThreadBlocker &blocker;
	std::thread &query_thread;
	bool released = false;
};

class CopyBackpressureBuffer : public AsyncWriteBuffer {
public:
	explicit CopyBackpressureBuffer(shared_ptr<string> data_p) : data(std::move(data_p)) {
	}

	data_ptr_t Ptr() override {
		return data_ptr_cast(data->data());
	}

	idx_t Size() const override {
		return data->size();
	}

private:
	shared_ptr<string> data;
};

struct CopyManagedGlobalData : GlobalFunctionData {
	CopyManagedGlobalData(AsyncFileWriter &writer_p, CopyAbortTestInfo &info_p) : writer(writer_p), info(info_p) {
	}

	~CopyManagedGlobalData() override {
		++info.destroy_count;
	}

	AsyncFileWriter &writer;
	CopyAbortTestInfo &info;
};

static unique_ptr<FunctionData> CopyAbortBind(ClientContext &, CopyFunctionBindInput &input, const vector<Identifier> &,
                                              const vector<LogicalType> &) {
	return make_uniq<CopyAbortBindData>(input.function_info);
}

static unique_ptr<LocalFunctionData> CopyAbortInitializeLocal(ExecutionContext &, FunctionData &) {
	return make_uniq<CopyAbortLocalData>();
}

static unique_ptr<GlobalFunctionData> CopyAbortInitializeGlobal(ClientContext &context, FunctionData &bind_data,
                                                                const string &path) {
	auto &info = bind_data.Cast<CopyAbortBindData>().GetInfo();
	++info.initialize_count;
	while (info.block_initialize && !info.release_initialize) {
		std::this_thread::yield();
	}
	unique_ptr<FileHandle> handle;
	if (info.create_file) {
		auto &fs = FileSystem::GetFileSystem(context);
		handle = fs.OpenFile(path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
	}
	if (info.fail_initialize) {
		throw IOException("Injected COPY initialize failure");
	}
	return make_uniq<CopyAbortGlobalData>(std::move(handle));
}

static unique_ptr<GlobalFunctionData> CopyManagedInitializeGlobal(ClientContext &, FunctionData &bind_data,
                                                                  CopyFunctionOutput &output) {
	auto &info = bind_data.Cast<CopyAbortBindData>().GetInfo();
	++info.initialize_count;
	if (info.fail_before_open) {
		throw IOException("Injected managed COPY failure before open");
	}
	if (info.skip_open) {
		return make_uniq<CopyAbortGlobalData>(nullptr);
	}
	auto &writer = output.Open();
	if (info.double_open) {
		output.Open();
	}
	if (info.fail_after_open) {
		throw IOException("Injected managed COPY failure after open");
	}
	if (info.return_empty_state) {
		return nullptr;
	}
	return make_uniq<CopyManagedGlobalData>(writer, info);
}

static void CopyAbortSink(ExecutionContext &, FunctionData &bind_data, GlobalFunctionData &, LocalFunctionData &,
                          DataChunk &) {
	if (bind_data.Cast<CopyAbortBindData>().GetInfo().fail_sink) {
		throw IOException("Injected COPY sink failure");
	}
}

static void CopyAbortCombine(ExecutionContext &, FunctionData &, GlobalFunctionData &, LocalFunctionData &) {
}

static void CopyManagedSink(ExecutionContext &, FunctionData &bind_data, GlobalFunctionData &global_data,
                            LocalFunctionData &, DataChunk &) {
	auto value = data_t('x');
	global_data.Cast<CopyManagedGlobalData>().writer.WriteData(&value, 1);
	if (bind_data.Cast<CopyAbortBindData>().GetInfo().fail_sink) {
		throw IOException("Injected managed COPY sink failure");
	}
}

static void CopyAbortFinalize(ClientContext &, FunctionData &bind_data, GlobalFunctionData &global_data) {
	auto &info = bind_data.Cast<CopyAbortBindData>().GetInfo();
	++info.finalize_count;
	if (info.close_in_finalize) {
		global_data.Cast<CopyManagedGlobalData>().writer.Close();
	}
	if (info.fail_finalize) {
		throw IOException("Injected COPY finalize failure");
	}
}

static CopyFunctionExecutionMode CopyAbortBatchExecutionMode(bool, bool) {
	return CopyFunctionExecutionMode::BATCH_COPY_TO_FILE;
}

static CopyFunctionExecutionMode CopyAbortRegularExecutionMode(bool, bool) {
	return CopyFunctionExecutionMode::REGULAR_COPY_TO_FILE;
}

static unique_ptr<PreparedBatchData> CopyAbortPrepareBatch(ClientContext &, FunctionData &, GlobalFunctionData &,
                                                           unique_ptr<ColumnDataCollection>) {
	return make_uniq<CopyAbortPreparedData>();
}

static void CopyAbortFlushBatch(ClientContext &, FunctionData &bind_data, GlobalFunctionData &, PreparedBatchData &) {
	if (bind_data.Cast<CopyAbortBindData>().GetInfo().fail_sink) {
		throw IOException("Injected COPY batch flush failure");
	}
}

static void CopyManagedFlushBatch(ClientContext &, FunctionData &bind_data, GlobalFunctionData &global_data,
                                  PreparedBatchData &) {
	auto &info = bind_data.Cast<CopyAbortBindData>().GetInfo();
	auto &writer = global_data.Cast<CopyManagedGlobalData>().writer;
	if (info.managed_write_data) {
		writer.WriteData(make_uniq<CopyBackpressureBuffer>(info.managed_write_data));
	} else {
		auto value = data_t('x');
		writer.WriteData(&value, 1);
	}
	info.flush_count++;
	if (info.fail_sink) {
		throw IOException("Injected managed COPY batch flush failure");
	}
}

static void CopyGetInvalidStatistics(ClientContext &, FunctionData &, GlobalFunctionData &,
                                     CopyFunctionFileStatistics &statistics) {
	statistics.footer_size_bytes = Value("not an integer");
}

static CopyFunction CreateCopyAbortFunction(const string &name, const shared_ptr<CopyAbortTestInfo> &info) {
	CopyFunction function {Identifier(name)};
	function.copy_to_bind = CopyAbortBind;
	function.copy_to_initialize_local = CopyAbortInitializeLocal;
	function.copy_to_initialize_global = CopyAbortInitializeGlobal;
	function.copy_to_sink = CopyAbortSink;
	function.copy_to_combine = CopyAbortCombine;
	function.copy_to_finalize = CopyAbortFinalize;
	function.function_info = info;
	return function;
}

static CopyFunction CreateManagedCopyFunction(const string &name, const shared_ptr<CopyAbortTestInfo> &info) {
	auto function = CreateCopyAbortFunction(name, info);
	function.copy_to_initialize_global = nullptr;
	function.copy_to_initialize_global_managed = CopyManagedInitializeGlobal;
	function.copy_to_sink = CopyManagedSink;
	return function;
}

static void TestManagedCopyBackpressure(CopyFunctionExecutionMode execution_mode) {
	DuckDB db(nullptr);
	Connection connection(db);
	auto thread_count = execution_mode == CopyFunctionExecutionMode::BATCH_COPY_TO_FILE ? 2 : 1;
	REQUIRE_NO_FAIL(connection.Query("SET threads=" + to_string(thread_count)));
	REQUIRE_NO_FAIL(connection.Query("SET async_threads=1"));
	constexpr idx_t TOTAL_FLUSHES = 10;
	constexpr idx_t TOTAL_ROWS = TOTAL_FLUSHES * STANDARD_VECTOR_SIZE;
	REQUIRE_NO_FAIL(
	    connection.Query(StringUtil::Format("CREATE TABLE copy_backpressure_input AS FROM range(%d)", TOTAL_ROWS)));
	auto &fs = FileSystem::GetFileSystem(*connection.context);
	auto local_fs = FileSystem::CreateLocal();
	string mode_name = execution_mode == CopyFunctionExecutionMode::BATCH_COPY_TO_FILE ? "batch" : "regular";
	auto actual_path = TestCreatePath("copy_backpressure_" + mode_name + ".test");
	local_fs->TryRemoveFile(actual_path);
	auto backpressure_fs = make_uniq<CopyBackpressureFileSystem>(actual_path);
	auto &backpressure_fs_ref = *backpressure_fs;
	fs.RegisterSubSystem(std::move(backpressure_fs));

	ExtensionInfo extension_info {};
	ExtensionActiveLoad load_info {*db.instance, extension_info, "copy_backpressure_test", ""};
	ExtensionLoader loader {load_info};
	auto info = make_shared_ptr<CopyAbortTestInfo>(false);
	info->managed_write_data = make_shared_ptr<string>(2 * AsyncWriteConfig::REMOTE_COALESCE_THRESHOLD, data_t('x'));
	auto function_name = "copy_backpressure_" + mode_name;
	auto function = CreateManagedCopyFunction(function_name, info);
	function.execution_mode = execution_mode == CopyFunctionExecutionMode::BATCH_COPY_TO_FILE
	                              ? CopyAbortBatchExecutionMode
	                              : CopyAbortRegularExecutionMode;
	function.prepare_batch = CopyAbortPrepareBatch;
	function.flush_batch = CopyManagedFlushBatch;
	loader.RegisterFunction(std::move(function));

	CopyAsyncThreadBlocker async_thread_blocker(*connection.context);
	auto async_worker_blocked = async_thread_blocker.WaitForStarted();
	unique_ptr<QueryResult> result;
	std::thread query_thread([&]() {
		result = connection.Query(StringUtil::Format("COPY copy_backpressure_input TO 'copy-backpressure://output' "
		                                             "(FORMAT %s, BATCH_SIZE 1, USE_TMP_FILE false)",
		                                             function_name));
	});
	CopyBackpressureRunGuard run_guard(backpressure_fs_ref, async_thread_blocker, query_thread);
	auto query_thread_entered_write = async_worker_blocked && backpressure_fs_ref.WaitForWrite();
	auto flush_count_at_write = info->flush_count.load();
	run_guard.Release();

	REQUIRE(async_worker_blocked);
	REQUIRE(query_thread_entered_write);
	REQUIRE(flush_count_at_write < TOTAL_FLUSHES);
	REQUIRE(result);
	REQUIRE_NO_FAIL(*result);
	REQUIRE(info->flush_count == TOTAL_FLUSHES);
	REQUIRE(backpressure_fs_ref.WriteCount() == TOTAL_FLUSHES);
	REQUIRE(backpressure_fs_ref.WrittenBytes() == TOTAL_FLUSHES * info->managed_write_data->size());
	REQUIRE(local_fs->FileExists(actual_path));
	local_fs->RemoveFile(actual_path);
}

static void TestManagedAmbiguousPublication(CopyFunctionExecutionMode execution_mode, bool close_in_finalize) {
	DuckDB db(nullptr);
	Connection connection(db);
	auto batch_mode = execution_mode == CopyFunctionExecutionMode::BATCH_COPY_TO_FILE;
	REQUIRE_NO_FAIL(connection.Query(batch_mode ? "SET threads=4" : "SET threads=1"));
	REQUIRE_NO_FAIL(connection.Query("SET async_threads=0"));
	if (batch_mode) {
		REQUIRE_NO_FAIL(connection.Query("CREATE TABLE copy_ambiguous_publish_input AS FROM range(4096)"));
	}

	auto &fs = FileSystem::GetFileSystem(*connection.context);
	auto publish_fs = make_uniq<CopyAmbiguousPublishFileSystem>();
	auto &publish_fs_ref = *publish_fs;
	fs.RegisterSubSystem(std::move(publish_fs));

	ExtensionInfo extension_info {};
	ExtensionActiveLoad load_info {*db.instance, extension_info, "copy_ambiguous_publish_test", ""};
	ExtensionLoader loader {load_info};
	auto info = make_shared_ptr<CopyAbortTestInfo>(false);
	info->close_in_finalize = close_in_finalize;
	auto mode_name = batch_mode ? "batch" : "regular";
	auto close_name = close_in_finalize ? "callback" : "operator";
	auto function_name = StringUtil::Format("copy_ambiguous_publish_%s_%s", mode_name, close_name);
	auto function = CreateManagedCopyFunction(function_name, info);
	function.execution_mode = batch_mode ? CopyAbortBatchExecutionMode : CopyAbortRegularExecutionMode;
	if (batch_mode) {
		function.prepare_batch = CopyAbortPrepareBatch;
		function.flush_batch = CopyManagedFlushBatch;
	}
	loader.RegisterFunction(std::move(function));

	auto source = batch_mode ? "copy_ambiguous_publish_input" : "(SELECT 1)";
	auto result = connection.Query(StringUtil::Format(
	    "COPY %s TO 'copy-ambiguous-publish://output' (FORMAT %s, USE_TMP_FILE false)", source, function_name));
	REQUIRE(result->HasError());
	INFO(result->GetError());
	REQUIRE(result->GetError().find("Injected ambiguous COPY publish failure") != string::npos);
	REQUIRE(publish_fs_ref.publish_count == 1);
	REQUIRE(publish_fs_ref.remove_count == 0);
	REQUIRE(publish_fs_ref.abort_count == 0);
	REQUIRE(publish_fs_ref.published);
	REQUIRE(publish_fs_ref.written_bytes > 0);
}

} // namespace

TEST_CASE("Managed COPY applies backpressure at physical flush boundaries", "[api][copy]") {
	TestManagedCopyBackpressure(CopyFunctionExecutionMode::BATCH_COPY_TO_FILE);
	TestManagedCopyBackpressure(CopyFunctionExecutionMode::REGULAR_COPY_TO_FILE);
}

TEST_CASE("Managed COPY preserves ambiguously published output", "[api][copy]") {
	TestManagedAmbiguousPublication(CopyFunctionExecutionMode::BATCH_COPY_TO_FILE, false);
	TestManagedAmbiguousPublication(CopyFunctionExecutionMode::REGULAR_COPY_TO_FILE, false);
	TestManagedAmbiguousPublication(CopyFunctionExecutionMode::BATCH_COPY_TO_FILE, true);
	TestManagedAmbiguousPublication(CopyFunctionExecutionMode::REGULAR_COPY_TO_FILE, true);
}

TEST_CASE("Legacy COPY preserves failed outputs", "[api][copy]") {
	DuckDB db(nullptr);
	Connection connection(db);
	REQUIRE_NO_FAIL(connection.Query("SET threads=1"));
	auto &fs = FileSystem::GetFileSystem(*connection.context);

	ExtensionInfo extension_info {};
	ExtensionActiveLoad load_info {*db.instance, extension_info, "copy_abort_fallback_test", ""};
	ExtensionLoader loader {load_info};

	auto failure_path = TestCreatePath("copy_abort_fallback_failure.test");
	fs.TryRemoveFile(failure_path);
	auto failure_info = make_shared_ptr<CopyAbortTestInfo>(true, false, false, true);
	auto failure_function = CreateCopyAbortFunction("copy_abort_fallback_failure", failure_info);
	loader.RegisterFunction(std::move(failure_function));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_abort_fallback_failure)", failure_path)));
	REQUIRE(fs.FileExists(failure_path));
	fs.RemoveFile(failure_path);

	auto batch_failure_path = TestCreatePath("copy_abort_fallback_batch_failure.test");
	fs.TryRemoveFile(batch_failure_path);
	auto batch_failure_info = make_shared_ptr<CopyAbortTestInfo>(true, false, false, true);
	auto batch_failure_function = CreateCopyAbortFunction("copy_abort_fallback_batch_failure", batch_failure_info);
	batch_failure_function.execution_mode = CopyAbortBatchExecutionMode;
	batch_failure_function.prepare_batch = CopyAbortPrepareBatch;
	batch_failure_function.flush_batch = CopyAbortFlushBatch;
	loader.RegisterFunction(std::move(batch_failure_function));
	REQUIRE_NO_FAIL(connection.Query("SET threads=4"));
	REQUIRE_NO_FAIL(connection.Query("CREATE TABLE copy_abort_fallback_batch_table AS FROM range(4096)"));
	REQUIRE_FAIL(connection.Query(
	    StringUtil::Format("COPY copy_abort_fallback_batch_table TO '%s' (FORMAT copy_abort_fallback_batch_failure)",
	                       batch_failure_path)));
	REQUIRE(fs.FileExists(batch_failure_path));
	fs.RemoveFile(batch_failure_path);
	REQUIRE_NO_FAIL(connection.Query("SET threads=1"));

	auto initialize_failure_path = TestCreatePath("copy_abort_fallback_initialize_failure.test");
	fs.TryRemoveFile(initialize_failure_path);
	auto initialize_failure_info = make_shared_ptr<CopyAbortTestInfo>(false, false, false, true);
	initialize_failure_info->fail_initialize = true;
	auto initialize_failure_function =
	    CreateCopyAbortFunction("copy_abort_fallback_initialize_failure", initialize_failure_info);
	loader.RegisterFunction(std::move(initialize_failure_function));
	REQUIRE_FAIL(connection.Query(
	    StringUtil::Format("COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_abort_fallback_initialize_failure)",
	                       initialize_failure_path)));
	REQUIRE(fs.FileExists(initialize_failure_path));
	fs.RemoveFile(initialize_failure_path);

	auto existing_path = TestCreatePath("copy_abort_fallback_existing.test");
	fs.TryRemoveFile(existing_path);
	{
		auto handle = fs.OpenFile(existing_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		handle->Close();
	}
	auto existing_info = make_shared_ptr<CopyAbortTestInfo>(true);
	auto existing_function = CreateCopyAbortFunction("copy_abort_fallback_existing", existing_info);
	loader.RegisterFunction(std::move(existing_function));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_abort_fallback_existing, USE_TMP_FILE false)",
	    existing_path)));
	REQUIRE(fs.FileExists(existing_path));
	fs.RemoveFile(existing_path);

	auto finalize_failure_path = TestCreatePath("copy_abort_fallback_finalize_failure.test");
	fs.TryRemoveFile(finalize_failure_path);
	auto finalize_failure_info = make_shared_ptr<CopyAbortTestInfo>(false, true, false, true);
	loader.RegisterFunction(CreateCopyAbortFunction("copy_abort_fallback_finalize_failure", finalize_failure_info));
	REQUIRE_FAIL(connection.Query(
	    StringUtil::Format("COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_abort_fallback_finalize_failure)",
	                       finalize_failure_path)));
	REQUIRE(finalize_failure_info->finalize_count == 1);
	REQUIRE(fs.FileExists(finalize_failure_path));
	fs.RemoveFile(finalize_failure_path);

	auto test_source_failure = [&](const string &name, bool batch_mode) {
		REQUIRE_NO_FAIL(connection.Query(batch_mode ? "SET threads=4" : "SET threads=1"));
		auto path = TestCreatePath(name + ".test");
		fs.TryRemoveFile(path);
		auto info = make_shared_ptr<CopyAbortTestInfo>(false, false, false, true);
		auto function = CreateCopyAbortFunction(name, info);
		function.copy_to_get_written_statistics = CopyGetInvalidStatistics;
		if (batch_mode) {
			function.execution_mode = CopyAbortBatchExecutionMode;
			function.prepare_batch = CopyAbortPrepareBatch;
			function.flush_batch = CopyAbortFlushBatch;
		}
		loader.RegisterFunction(std::move(function));
		auto source = batch_mode ? "copy_abort_fallback_batch_table" : "(SELECT i FROM range(1) t(i))";
		REQUIRE_FAIL(
		    connection.Query(StringUtil::Format("COPY %s TO '%s' (FORMAT %s, RETURN_STATS)", source, path, name)));
		REQUIRE(info->finalize_count == 1);
		REQUIRE(fs.FileExists(path));
		fs.RemoveFile(path);
	};
	test_source_failure("copy_abort_fallback_regular_source_failure", false);
	test_source_failure("copy_abort_fallback_batch_source_failure", true);

	auto test_competing_owner = [&](const string &name, bool batch_mode) {
		REQUIRE_NO_FAIL(connection.Query(batch_mode ? "SET threads=4" : "SET threads=1"));
		auto path = TestCreatePath(name + ".test");
		fs.TryRemoveFile(path);
		auto info = make_shared_ptr<CopyAbortTestInfo>(true, false, true, true);
		auto function = CreateCopyAbortFunction(name, info);
		if (batch_mode) {
			function.execution_mode = CopyAbortBatchExecutionMode;
			function.prepare_batch = CopyAbortPrepareBatch;
			function.flush_batch = CopyAbortFlushBatch;
		}
		loader.RegisterFunction(std::move(function));
		unique_ptr<QueryResult> result;
		std::thread query_thread([&]() {
			auto source = batch_mode ? "copy_abort_fallback_batch_table" : "(SELECT i FROM range(1) t(i))";
			result = connection.Query(StringUtil::Format("COPY %s TO '%s' (FORMAT %s)", source, path, name));
		});
		bool initialize_started = false;
		for (idx_t i = 0; i < 1000000; i++) {
			if (info->initialize_count > 0) {
				initialize_started = true;
				break;
			}
			std::this_thread::yield();
		}
		if (initialize_started) {
			auto competing = fs.OpenFile(path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
			competing->Close();
		}
		info->release_initialize = true;
		query_thread.join();
		REQUIRE(initialize_started);
		REQUIRE(result);
		REQUIRE(result->HasError());
		REQUIRE(fs.FileExists(path));
		fs.RemoveFile(path);
	};
	test_competing_owner("copy_abort_fallback_regular_competing_owner", false);
	test_competing_owner("copy_abort_fallback_batch_competing_owner", true);

	auto success_path = TestCreatePath("copy_abort_fallback_success.test");
	fs.TryRemoveFile(success_path);
	auto success_info = make_shared_ptr<CopyAbortTestInfo>(false, false, false, true);
	auto success_function = CreateCopyAbortFunction("copy_abort_fallback_success", success_info);
	loader.RegisterFunction(std::move(success_function));
	REQUIRE_NO_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_abort_fallback_success)", success_path)));
	REQUIRE(fs.FileExists(success_path));
	fs.RemoveFile(success_path);
}

TEST_CASE("COPY manages operator-owned output writers", "[api][copy]") {
	DuckDB db(nullptr);
	Connection connection(db);
	REQUIRE_NO_FAIL(connection.Query("SET threads=1"));
	auto &fs = FileSystem::GetFileSystem(*connection.context);

	ExtensionInfo extension_info {};
	ExtensionActiveLoad load_info {*db.instance, extension_info, "copy_managed_output_test", ""};
	ExtensionLoader loader {load_info};

	auto sink_failure_path = TestCreatePath("copy_managed_sink_failure.test");
	fs.TryRemoveFile(sink_failure_path);
	auto sink_failure_info = make_shared_ptr<CopyAbortTestInfo>(true);
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_sink_failure", sink_failure_info));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_sink_failure, USE_TMP_FILE false)",
	    sink_failure_path)));
	REQUIRE(sink_failure_info->destroy_count == 1);
	REQUIRE(!fs.FileExists(sink_failure_path));

	auto finalize_failure_path = TestCreatePath("copy_managed_finalize_failure.test");
	fs.TryRemoveFile(finalize_failure_path);
	auto finalize_failure_info = make_shared_ptr<CopyAbortTestInfo>(false, true);
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_finalize_failure", finalize_failure_info));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_finalize_failure, USE_TMP_FILE false)",
	    finalize_failure_path)));
	REQUIRE(finalize_failure_info->finalize_count == 1);
	REQUIRE(finalize_failure_info->destroy_count == 1);
	REQUIRE(!fs.FileExists(finalize_failure_path));

	auto success_path = TestCreatePath("copy_managed_success.test");
	fs.TryRemoveFile(success_path);
	auto success_info = make_shared_ptr<CopyAbortTestInfo>(false);
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_success", success_info));
	REQUIRE_NO_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_success, USE_TMP_FILE false)", success_path)));
	REQUIRE(success_info->finalize_count == 1);
	REQUIRE(success_info->destroy_count == 1);
	REQUIRE(fs.FileExists(success_path));
	{
		auto handle = fs.OpenFile(success_path, FileFlags::FILE_FLAGS_READ);
		REQUIRE(handle->GetFileSize() > 0);
	}
	fs.RemoveFile(success_path);

	auto existing_path = TestCreatePath("copy_managed_existing.test");
	fs.TryRemoveFile(existing_path);
	{
		auto handle = fs.OpenFile(existing_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		handle->Close();
	}
	auto existing_info = make_shared_ptr<CopyAbortTestInfo>(true);
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_existing", existing_info));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_existing, USE_TMP_FILE false)",
	    existing_path)));
	REQUIRE(fs.FileExists(existing_path));
	fs.RemoveFile(existing_path);

	auto before_open_path = TestCreatePath("copy_managed_before_open.test");
	fs.TryRemoveFile(before_open_path);
	auto before_open_info = make_shared_ptr<CopyAbortTestInfo>(false);
	before_open_info->fail_before_open = true;
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_before_open", before_open_info));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_before_open, USE_TMP_FILE false)",
	    before_open_path)));
	REQUIRE(!fs.FileExists(before_open_path));

	auto after_open_path = TestCreatePath("copy_managed_after_open.test");
	fs.TryRemoveFile(after_open_path);
	auto after_open_info = make_shared_ptr<CopyAbortTestInfo>(false);
	after_open_info->fail_after_open = true;
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_after_open", after_open_info));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_after_open, USE_TMP_FILE false)",
	    after_open_path)));
	REQUIRE(!fs.FileExists(after_open_path));

	auto no_open_path = TestCreatePath("copy_managed_no_open.test");
	fs.TryRemoveFile(no_open_path);
	auto no_open_info = make_shared_ptr<CopyAbortTestInfo>(false);
	no_open_info->skip_open = true;
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_no_open", no_open_info));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_no_open, USE_TMP_FILE false)", no_open_path)));
	REQUIRE(!fs.FileExists(no_open_path));

	auto double_open_path = TestCreatePath("copy_managed_double_open.test");
	fs.TryRemoveFile(double_open_path);
	auto double_open_info = make_shared_ptr<CopyAbortTestInfo>(false);
	double_open_info->double_open = true;
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_double_open", double_open_info));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_double_open, USE_TMP_FILE false)",
	    double_open_path)));
	REQUIRE(!fs.FileExists(double_open_path));

	auto empty_state_path = TestCreatePath("copy_managed_empty_state.test");
	fs.TryRemoveFile(empty_state_path);
	auto empty_state_info = make_shared_ptr<CopyAbortTestInfo>(false);
	empty_state_info->return_empty_state = true;
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_empty_state", empty_state_info));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(1) t(i)) TO '%s' (FORMAT copy_managed_empty_state, USE_TMP_FILE false)",
	    empty_state_path)));
	REQUIRE(!fs.FileExists(empty_state_path));

	auto batch_failure_path = TestCreatePath("copy_managed_batch_failure.test");
	fs.TryRemoveFile(batch_failure_path);
	auto batch_failure_info = make_shared_ptr<CopyAbortTestInfo>(true);
	auto batch_failure_function = CreateManagedCopyFunction("copy_managed_batch_failure", batch_failure_info);
	batch_failure_function.execution_mode = CopyAbortBatchExecutionMode;
	batch_failure_function.prepare_batch = CopyAbortPrepareBatch;
	batch_failure_function.flush_batch = CopyManagedFlushBatch;
	loader.RegisterFunction(std::move(batch_failure_function));
	REQUIRE_NO_FAIL(connection.Query("SET threads=4"));
	REQUIRE_NO_FAIL(connection.Query("CREATE TABLE copy_managed_batch_table AS FROM range(4096)"));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY copy_managed_batch_table TO '%s' (FORMAT copy_managed_batch_failure, USE_TMP_FILE false)",
	    batch_failure_path)));
	REQUIRE(batch_failure_info->destroy_count == 1);
	REQUIRE(!fs.FileExists(batch_failure_path));

	auto batch_success_path = TestCreatePath("copy_managed_batch_success.test");
	fs.TryRemoveFile(batch_success_path);
	auto batch_success_info = make_shared_ptr<CopyAbortTestInfo>(false);
	auto batch_success_function = CreateManagedCopyFunction("copy_managed_batch_success", batch_success_info);
	batch_success_function.execution_mode = CopyAbortBatchExecutionMode;
	batch_success_function.prepare_batch = CopyAbortPrepareBatch;
	batch_success_function.flush_batch = CopyManagedFlushBatch;
	loader.RegisterFunction(std::move(batch_success_function));
	REQUIRE_NO_FAIL(connection.Query(StringUtil::Format(
	    "COPY copy_managed_batch_table TO '%s' (FORMAT copy_managed_batch_success, USE_TMP_FILE false)",
	    batch_success_path)));
	REQUIRE(batch_success_info->destroy_count == 1);
	REQUIRE(fs.FileExists(batch_success_path));
	fs.RemoveFile(batch_success_path);

	auto batch_existing_path = TestCreatePath("copy_managed_batch_existing.test");
	fs.TryRemoveFile(batch_existing_path);
	{
		auto handle =
		    fs.OpenFile(batch_existing_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		handle->Close();
	}
	auto batch_existing_info = make_shared_ptr<CopyAbortTestInfo>(true);
	auto batch_existing_function = CreateManagedCopyFunction("copy_managed_batch_existing", batch_existing_info);
	batch_existing_function.execution_mode = CopyAbortBatchExecutionMode;
	batch_existing_function.prepare_batch = CopyAbortPrepareBatch;
	batch_existing_function.flush_batch = CopyManagedFlushBatch;
	loader.RegisterFunction(std::move(batch_existing_function));
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY copy_managed_batch_table TO '%s' (FORMAT copy_managed_batch_existing, USE_TMP_FILE false)",
	    batch_existing_path)));
	REQUIRE(batch_existing_info->destroy_count == 1);
	REQUIRE(fs.FileExists(batch_existing_path));
	fs.RemoveFile(batch_existing_path);

	auto test_source_failure = [&](const string &name, bool batch_mode, bool existing) {
		auto path = TestCreatePath(name + ".test");
		fs.TryRemoveFile(path);
		if (existing) {
			auto handle = fs.OpenFile(path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
			handle->Close();
		}
		auto info = make_shared_ptr<CopyAbortTestInfo>(false);
		auto function = CreateManagedCopyFunction(name, info);
		function.copy_to_get_written_statistics = CopyGetInvalidStatistics;
		if (batch_mode) {
			function.execution_mode = CopyAbortBatchExecutionMode;
			function.prepare_batch = CopyAbortPrepareBatch;
			function.flush_batch = CopyManagedFlushBatch;
		}
		loader.RegisterFunction(std::move(function));
		auto source = batch_mode ? "copy_managed_batch_table" : "(SELECT i FROM range(1) t(i))";
		REQUIRE_FAIL(connection.Query(
		    StringUtil::Format("COPY %s TO '%s' (FORMAT %s, USE_TMP_FILE false, RETURN_STATS)", source, path, name)));
		REQUIRE(info->finalize_count == 1);
		REQUIRE(info->destroy_count == 1);
		REQUIRE(fs.FileExists(path) == existing);
		fs.TryRemoveFile(path);
	};
	test_source_failure("copy_managed_regular_source_failure", false, false);
	test_source_failure("copy_managed_regular_existing_source_failure", false, true);
	test_source_failure("copy_managed_batch_source_failure", true, false);
	test_source_failure("copy_managed_batch_existing_source_failure", true, true);
}

TEST_CASE("Managed COPY preserves a competing exclusive-create winner", "[api][copy]") {
	DuckDB db(nullptr);
	Connection connection(db);
	auto &fs = FileSystem::GetFileSystem(*connection.context);
	auto local_fs = FileSystem::CreateLocal();
	auto actual_path = TestCreatePath("copy_managed_race_winner.test");
	local_fs->TryRemoveFile(actual_path);
	fs.RegisterSubSystem(make_uniq<CopyManagedRaceFileSystem>(actual_path));

	ExtensionInfo extension_info {};
	ExtensionActiveLoad load_info {*db.instance, extension_info, "copy_managed_race_test", ""};
	ExtensionLoader loader {load_info};
	auto info = make_shared_ptr<CopyAbortTestInfo>(false);
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_race", info));

	REQUIRE_FAIL(connection.Query(
	    "COPY (SELECT 1) TO 'copy-managed-race://output' (FORMAT copy_managed_race, USE_TMP_FILE false)"));
	REQUIRE(local_fs->FileExists(actual_path));
	local_fs->RemoveFile(actual_path);
}

TEST_CASE("Managed COPY lifecycle removes output for a close-only filesystem", "[api][copy]") {
	DuckDB db(nullptr);
	Connection connection(db);
	auto &fs = FileSystem::GetFileSystem(*connection.context);
	auto local_fs = FileSystem::CreateLocal();
	auto actual_path = TestCreatePath("copy_managed_close_only.test");
	local_fs->TryRemoveFile(actual_path);
	fs.RegisterSubSystem(make_uniq<CopyManagedCloseOnlyFileSystem>(actual_path));

	ExtensionInfo extension_info {};
	ExtensionActiveLoad load_info {*db.instance, extension_info, "copy_managed_close_only_test", ""};
	ExtensionLoader loader {load_info};
	auto failure_info = make_shared_ptr<CopyAbortTestInfo>(true);
	loader.RegisterFunction(CreateManagedCopyFunction("copy_managed_close_only", failure_info));

	REQUIRE_FAIL(connection.Query(
	    "COPY (SELECT 1) TO 'copy-managed-close-only://output' (FORMAT copy_managed_close_only, USE_TMP_FILE false)"));
	REQUIRE(!local_fs->FileExists(actual_path));
}

TEST_CASE("COPY validates managed output initialization", "[api][copy]") {
	DuckDB db(nullptr);
	Connection connection(db);

	ExtensionInfo extension_info {};
	ExtensionActiveLoad load_info {*db.instance, extension_info, "copy_managed_validation_test", ""};
	ExtensionLoader loader {load_info};

	auto info = make_shared_ptr<CopyAbortTestInfo>(false);
	auto both = CreateManagedCopyFunction("copy_managed_both", info);
	both.copy_to_initialize_global = CopyAbortInitializeGlobal;
	loader.RegisterFunction(std::move(both));
	auto both_result = connection.Query("COPY (SELECT 1) TO 'copy_managed_both.test' (FORMAT copy_managed_both)");
	REQUIRE(both_result->HasError());
	REQUIRE(StringUtil::Contains(both_result->GetError(), "exactly one global initializer"));

	auto neither = CreateManagedCopyFunction("copy_managed_neither", info);
	neither.copy_to_initialize_global_managed = nullptr;
	loader.RegisterFunction(std::move(neither));
	auto neither_result =
	    connection.Query("COPY (SELECT 1) TO 'copy_managed_neither.test' (FORMAT copy_managed_neither)");
	REQUIRE(neither_result->HasError());
	REQUIRE(StringUtil::Contains(neither_result->GetError(), "exactly one global initializer"));
}

TEST_CASE("COPY output cleanup preserves ownership across commit and temporary moves", "[api][copy]") {
	DuckDB db(nullptr);
	Connection connection(db);
	auto &fs = FileSystem::GetFileSystem(*connection.context);
	auto committed_path = TestCreatePath("copy_output_committed_failure.test");
	fs.TryRemoveFile(committed_path);
	{
		CopyOutputLifecycle lifecycle(*connection.context);
		auto file_index = lifecycle.RegisterFile(committed_path);
		auto handle = fs.OpenFile(committed_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		handle->Close();
		lifecycle.MarkFileCommitted(file_index);
	}
	REQUIRE(fs.FileExists(committed_path));
	fs.RemoveFile(committed_path);

	auto existing_path = TestCreatePath("copy_output_existing_committed_failure.test");
	fs.TryRemoveFile(existing_path);
	{
		auto handle = fs.OpenFile(existing_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		handle->Close();
	}
	{
		CopyOutputLifecycle lifecycle(*connection.context);
		auto file_index = lifecycle.RegisterFile(existing_path);
		lifecycle.MarkFileCommitted(file_index);
	}
	REQUIRE(fs.FileExists(existing_path));
	fs.RemoveFile(existing_path);

	auto temporary_path = TestCreatePath("tmp_copy_output_move.test");
	auto temporary_target = TestCreatePath("copy_output_move.test");
	fs.TryRemoveFile(temporary_path);
	fs.TryRemoveFile(temporary_target);
	{
		CopyOutputLifecycle lifecycle(*connection.context);
		auto file_index = lifecycle.RegisterFile(temporary_path);
		auto handle = fs.OpenFile(temporary_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		handle->Close();
		lifecycle.MarkFileCommitted(file_index);
		lifecycle.CommitTemporaryFile(temporary_path);
	}
	REQUIRE(!fs.FileExists(temporary_target));

	fs.TryRemoveFile(temporary_path);
	{
		auto handle =
		    fs.OpenFile(temporary_target, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		handle->Close();
	}
	{
		CopyOutputLifecycle lifecycle(*connection.context);
		auto file_index = lifecycle.RegisterFile(temporary_path);
		auto handle = fs.OpenFile(temporary_path, FileFlags::FILE_FLAGS_WRITE | FileFlags::FILE_FLAGS_FILE_CREATE_NEW);
		handle->Close();
		lifecycle.MarkFileCommitted(file_index);
		lifecycle.CommitTemporaryFile(temporary_path);
	}
	REQUIRE(fs.FileExists(temporary_target));
	fs.RemoveFile(temporary_target);
}

TEST_CASE("COPY cleans failed file and directory outputs", "[api][copy]") {
	DuckDB db(nullptr);
	Connection connection(db);
	REQUIRE_NO_FAIL(connection.Query("SET threads=1"));
	auto &fs = FileSystem::GetFileSystem(*connection.context);

	auto batch_path = TestCreatePath("copy_abort_batch.csv");
	fs.TryRemoveFile(batch_path);
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT CASE WHEN i=4096 THEN error('injected') ELSE i::VARCHAR END AS v FROM range(8192) t(i)) "
	    "TO '%s' (FORMAT CSV)",
	    batch_path)));
	REQUIRE(!fs.FileExists(batch_path));

	auto blob_path = TestCreatePath("copy_abort_regular.blob");
	fs.TryRemoveFile(blob_path);
	REQUIRE_FAIL(connection.Query(
	    StringUtil::Format("COPY (SELECT CASE WHEN i=4096 THEN error('injected') ELSE repeat('x', 100)::BLOB END AS v "
	                       "FROM range(8192) t(i)) TO '%s' (FORMAT BLOB)",
	                       blob_path)));
	REQUIRE(!fs.FileExists(blob_path));

	auto compressed_blob_path = TestCreatePath("copy_abort_regular.blob.gz");
	fs.TryRemoveFile(compressed_blob_path);
	REQUIRE_FAIL(connection.Query(
	    StringUtil::Format("COPY (SELECT CASE WHEN i=4096 THEN error('injected') ELSE repeat('x', 100)::BLOB END AS v "
	                       "FROM range(8192) t(i)) TO '%s' (FORMAT BLOB, COMPRESSION GZIP)",
	                       compressed_blob_path)));
	REQUIRE(!fs.FileExists(compressed_blob_path));

	auto parquet_path = TestCreatePath("copy_abort_regular.parquet");
	fs.TryRemoveFile(parquet_path);
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT CASE WHEN i=4096 THEN error('injected') ELSE i END AS v FROM range(8192) t(i)) "
	    "TO '%s' (FORMAT PARQUET, PRESERVE_ORDER false)",
	    parquet_path)));
	REQUIRE(!fs.FileExists(parquet_path));

	auto rotated_directory = TestCreatePath("copy_abort_rotated");
	if (fs.DirectoryExists(rotated_directory)) {
		fs.RemoveDirectory(rotated_directory);
	}
	REQUIRE_FAIL(connection.Query(
	    StringUtil::Format("COPY (SELECT CASE WHEN i=8192 THEN error('injected') ELSE repeat('x', 100) END AS v "
	                       "FROM range(16384) t(i)) TO '%s' "
	                       "(FORMAT CSV, FILE_SIZE_BYTES '16KB', PRESERVE_ORDER false)",
	                       rotated_directory)));
	REQUIRE(!fs.DirectoryExists(rotated_directory));

	auto partitioned_directory = TestCreatePath("copy_abort_partitioned");
	if (fs.DirectoryExists(partitioned_directory)) {
		fs.RemoveDirectory(partitioned_directory);
	}
	REQUIRE_FAIL(connection.Query(
	    StringUtil::Format("COPY (SELECT i %% 4 AS p, CASE WHEN i=16384 THEN error('injected') "
	                       "ELSE repeat('x', 100) END AS v FROM range(32768) t(i)) TO '%s' "
	                       "(FORMAT CSV, PARTITION_BY (p), FILE_SIZE_BYTES '16KB', PRESERVE_ORDER false)",
	                       partitioned_directory)));
	REQUIRE(!fs.DirectoryExists(partitioned_directory));

	auto existing_directory = TestCreatePath("copy_abort_existing");
	if (fs.DirectoryExists(existing_directory)) {
		fs.RemoveDirectory(existing_directory);
	}
	fs.CreateDirectory(existing_directory);
	REQUIRE_FAIL(connection.Query(
	    StringUtil::Format("COPY (SELECT CASE WHEN i=8192 THEN error('injected') ELSE repeat('x', 100) END AS v "
	                       "FROM range(16384) t(i)) TO '%s' "
	                       "(FORMAT CSV, FILE_SIZE_BYTES '16KB', PRESERVE_ORDER false)",
	                       existing_directory)));
	REQUIRE(fs.DirectoryExists(existing_directory));
	idx_t remaining_files = 0;
	fs.ListFiles(existing_directory, [&](const string &, bool) { remaining_files++; });
	REQUIRE(remaining_files == 0);
	fs.RemoveDirectory(existing_directory);

	auto move_target = TestCreatePath("copy_abort_move.csv");
	auto move_tmp = fs.JoinPath(StringUtil::GetFilePath(move_target), "tmp_" + StringUtil::GetFileName(move_target));
	fs.TryRemoveFile(move_tmp);
	if (fs.DirectoryExists(move_target)) {
		fs.RemoveDirectory(move_target);
	}
	fs.CreateDirectory(move_target);
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT i FROM range(10) t(i)) TO '%s' (FORMAT CSV, USE_TMP_FILE true)", move_target)));
	REQUIRE(!fs.FileExists(move_tmp));
	REQUIRE(fs.DirectoryExists(move_target));
	fs.RemoveDirectory(move_target);

	auto blob_move_target = TestCreatePath("copy_abort_move.blob");
	auto blob_move_tmp =
	    fs.JoinPath(StringUtil::GetFilePath(blob_move_target), "tmp_" + StringUtil::GetFileName(blob_move_target));
	fs.TryRemoveFile(blob_move_tmp);
	if (fs.DirectoryExists(blob_move_target)) {
		fs.RemoveDirectory(blob_move_target);
	}
	fs.CreateDirectory(blob_move_target);
	REQUIRE_FAIL(connection.Query(StringUtil::Format(
	    "COPY (SELECT 'payload'::BLOB) TO '%s' (FORMAT BLOB, USE_TMP_FILE true)", blob_move_target)));
	REQUIRE(!fs.FileExists(blob_move_tmp));
	REQUIRE(fs.DirectoryExists(blob_move_target));
	fs.RemoveDirectory(blob_move_target);

	auto success_path = TestCreatePath("copy_abort_success.csv");
	fs.TryRemoveFile(success_path);
	REQUIRE_NO_FAIL(
	    connection.Query(StringUtil::Format("COPY (SELECT i FROM range(10) t(i)) TO '%s' (FORMAT CSV)", success_path)));
	REQUIRE(fs.FileExists(success_path));
	fs.RemoveFile(success_path);
}
