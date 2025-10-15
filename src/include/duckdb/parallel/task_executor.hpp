//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/task_executor.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/atomic.hpp"
#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/parallel/task.hpp"
#include "duckdb/execution/task_error_manager.hpp"

namespace duckdb {
class TaskScheduler;
class Event;

//! The TaskExecutor is a helper class that enables parallel scheduling and execution of tasks
class TaskExecutor {
public:
	explicit TaskExecutor(ClientContext &context);
	explicit TaskExecutor(TaskScheduler &scheduler);
	~TaskExecutor();

	//! Push an error into the TaskExecutor
	void PushError(ErrorData error);
	//! Whether or not any task has encountered an error
	bool HasError();
	//! Throw an error that was encountered during execution (if HasError() is true)
	void ThrowError();

	//! Schedule a new task
	void ScheduleTask(unique_ptr<Task> task);
	//! Schedule a new task, supplying a ProducerToken to be used
	void ScheduleTask(unique_ptr<Task> task, ProducerToken &alternative_token);
	//! Label a task as finished
	void FinishTask();

	//! Work on tasks until all tasks are finished. Throws an exception if any error occurred while executing the tasks.
	void WorkOnTasks();

	//! Get a task - returns true if a task was found
	bool GetTask(shared_ptr<Task> &task);

private:
	TaskScheduler &scheduler;
	TaskErrorManager error_manager;
	unique_ptr<ProducerToken> token;
	atomic<idx_t> completed_tasks;
	atomic<idx_t> total_tasks;
	friend class BaseExecutorTask;
	optional_ptr<ClientContext> context;
};

class BaseExecutorTask : public Task {
public:
	explicit BaseExecutorTask(TaskExecutor &executor);
	BaseExecutorTask(const BaseExecutorTask &other) = default;
	BaseExecutorTask(BaseExecutorTask &&other) = default;

	virtual void ExecuteTask() = 0;
	TaskExecutionResult Execute(TaskExecutionMode mode) override;

protected:
	TaskExecutor &executor;
};

class ToBeScheduledTask : public Task {
public:
	ToBeScheduledTask() = default;
	ToBeScheduledTask(const ToBeScheduledTask &other) = default;
	ToBeScheduledTask(ToBeScheduledTask &&other) = default;
	unique_ptr<BaseExecutorTask> Schedule(Executor &executor, shared_ptr<Event> event) {
		throw InternalException("ToBeScheduledTask cannot be call on base class");
	}
	string TaskType() const override {
		return "ToBeScheduledTask";
	}
	TaskExecutionResult Execute(TaskExecutionMode mode) override {
		throw InternalException("ASD");
	}
};

typedef void (*promise_compute)(void *);
typedef void (*promise_cleanup)(void *);

class Promise {
public:
	void *promise_state;
	promise_compute compute_callback;
	promise_cleanup compute_cleanup;
};

class PromiseHolder {
public:
	std::vector<unique_ptr<Promise>> v;
};

} // namespace duckdb
