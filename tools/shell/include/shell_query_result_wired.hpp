//===----------------------------------------------------------------------===//
//                         DuckDB
//
// shell_query_result_wired.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "shell_query_result.hpp"
#include "wire_transport.hpp"
#include "duckdb/common/optional_idx.hpp"

namespace duckdb_shell {

//! Wire-mode implementation of ShellQueryResult.
//! Holds metadata from the transport layer and fetches data on demand.
class ShellQueryResultWired : public ShellQueryResult {
public:
	ShellQueryResultWired(WireResultMetadata metadata, TransportLayer *transport);
	~ShellQueryResultWired() override;

	bool HasError() const override;
	const duckdb::string &GetError() const override;
	duckdb::StatementReturnType GetReturnType() const override;
	duckdb::QueryResultType GetResultType() const override;
	idx_t ColumnCount() const override;
	const duckdb::vector<duckdb::string> &Names() const override;
	const duckdb::vector<duckdb::LogicalType> &Types() const override;
	duckdb::unique_ptr<duckdb::DataChunk> Fetch() override;
	iterator begin() override;
	iterator end() override;

	//! Connection ID for Fetch() calls — set by ShellConnectionWired
	duckdb::optional_idx conn_id;

private:
	WireResultMetadata metadata;
	TransportLayer *transport;
	duckdb::vector<duckdb::LogicalType> types;
};

//! Wire-mode implementation of ShellMaterializedQueryResult.
class ShellMaterializedQueryResultWired : public ShellMaterializedQueryResult {
public:
	ShellMaterializedQueryResultWired(WireResultMetadata metadata, TransportLayer *transport);
	~ShellMaterializedQueryResultWired() override;

	bool HasError() const override;
	const duckdb::string &GetError() const override;
	duckdb::StatementReturnType GetReturnType() const override;
	duckdb::QueryResultType GetResultType() const override;
	idx_t ColumnCount() const override;
	const duckdb::vector<duckdb::string> &Names() const override;
	const duckdb::vector<duckdb::LogicalType> &Types() const override;
	duckdb::unique_ptr<duckdb::DataChunk> Fetch() override;
	iterator begin() override;
	iterator end() override;

	idx_t RowCount() const override;
	ShellColumnDataCollection Collection() override;

	//! Connection ID for Fetch() calls — set by ShellConnectionWired
	duckdb::optional_idx conn_id;

private:
	WireResultMetadata metadata;
	TransportLayer *transport;
	duckdb::vector<duckdb::LogicalType> types;
};

} // namespace duckdb_shell
