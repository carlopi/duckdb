//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/helper/physical_fan_out.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/execution/physical_operator.hpp"

namespace duckdb {

//! PhysicalFanOut wraps a sequential source and exposes it as a parallel source.
//! Multiple threads call GetData concurrently; access to the underlying source is
//! serialized with a mutex. Each chunk gets a monotonically increasing batch index
//! so downstream operators (and the BatchCollector) can reconstruct original order.
class PhysicalFanOut : public PhysicalOperator {
public:
	static constexpr const PhysicalOperatorType TYPE = PhysicalOperatorType::FAN_OUT;

	PhysicalFanOut(PhysicalPlan &plan, PhysicalOperator &child_source, idx_t estimated_cardinality);

	//! The wrapped sequential source
	reference<PhysicalOperator> child_source;

public:
	// Source interface
	bool IsSource() const override {
		return true;
	}
	bool ParallelSource() const override {
		return true;
	}

	unique_ptr<GlobalSourceState> GetGlobalSourceState(ClientContext &context) const override;
	unique_ptr<LocalSourceState> GetLocalSourceState(ExecutionContext &context,
	                                                 GlobalSourceState &gstate) const override;
	SourceResultType GetDataInternal(ExecutionContext &context, DataChunk &chunk,
	                                 OperatorSourceInput &input) const override;

	bool SupportsPartitioning(const OperatorPartitionInfo &partition_info) const override {
		return true;
	}
	OperatorPartitionData GetPartitionData(ExecutionContext &context, DataChunk &chunk, GlobalSourceState &gstate,
	                                       LocalSourceState &lstate,
	                                       const OperatorPartitionInfo &partition_info) const override;

	bool IsSink() const override {
		return false;
	}

	string GetName() const override {
		return "FAN_OUT";
	}

	InsertionOrderPreservingMap<string> ParamsToString() const override {
		InsertionOrderPreservingMap<string> result;
		result["Child"] = child_source.get().GetName();
		return result;
	}
};

} // namespace duckdb
