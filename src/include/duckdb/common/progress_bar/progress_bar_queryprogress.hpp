//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/progress_bar/progress_bar_queryprogress.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.h"
#include "duckdb/common/mutex.hpp"

namespace duckdb {

struct ClientConfig;

struct QueryProgress {
	friend class ProgressBar;

public:
	QueryProgress();
	void Initialize();
	void Restart();
	double GetPercentage();
	uint64_t GetRowsProcesseed();
	uint64_t GetTotalRowsToProcess();
	QueryProgress &operator=(const QueryProgress &other);
	QueryProgress(const QueryProgress &other);

private:
	atomic<double> percentage;
	atomic<uint64_t> rows_processed;
	atomic<uint64_t> total_rows_to_process;
};

} // namespace duckdb
