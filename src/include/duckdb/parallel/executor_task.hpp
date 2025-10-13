//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/executor_task.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/parallel/task.hpp"
#include "duckdb/parallel/task_executor.hpp"
#include "duckdb/common/optional_ptr.hpp"

namespace duckdb {
class Event;
class PhysicalOperator;
class ThreadContext;
class CachingFileHandle;

//! Execute a task within an executor, including exception handling
//! This should be used within queries
class ExecutorTask : public Task {
public:
	ExecutorTask(Executor &executor, shared_ptr<Event> event);
	ExecutorTask(ClientContext &context, shared_ptr<Event> event, const PhysicalOperator &op);
	~ExecutorTask() override;

public:
	void Deschedule() override;
	void Reschedule() override;

public:
	Executor &executor;
	shared_ptr<Event> event;
	unique_ptr<ThreadContext> thread_context;
	optional_ptr<const PhysicalOperator> op;

private:
	ClientContext &context;

public:
	virtual TaskExecutionResult ExecuteTask(TaskExecutionMode mode) = 0;
	TaskExecutionResult Execute(TaskExecutionMode mode) override;
};

class IOTask : public ExecutorTask {
public:
	explicit IOTask(Executor &executor, shared_ptr<Event> event, BufferHandle &buffer_handle, CachingFileHandle &handle,
	                idx_t start, idx_t end);

	BufferHandle &buffer_handle;
	CachingFileHandle &handle;
	idx_t start;
	idx_t end;
	string TaskType() const override {
		return "IOTask";
	}
	virtual TaskExecutionResult ExecuteTask(TaskExecutionMode mode) override {
		return TaskExecutionResult::TASK_FINISHED;
	}
};

class IOToBeScheduledTask : public ToBeScheduledTask {
public:
	explicit IOToBeScheduledTask(BufferHandle &buffer_handle, CachingFileHandle &handle, data_ptr_t &ptr, idx_t size,
	                             idx_t location, bool &x);
	unique_ptr<IOTask> Schedule(Executor &executor, shared_ptr<Event> event);

	BufferHandle &buffer_handle;
	CachingFileHandle &handle;
	data_ptr_t &ptr;
	idx_t size;
	idx_t location;
	bool &data_isset;
	string TaskType() const override {
		return "IOToBeScheduledTask";
	}
	TaskExecutionResult Execute(TaskExecutionMode mode) override {
		throw InternalException("BOOM");
	}
};

} // namespace duckdb
