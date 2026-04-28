#include "duckdb/optimizer/statistics_propagator.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/execution/expression_executor.hpp"

namespace duckdb {

// Range propagation through a monotonic function. For a function f declared monotonic
// in some args, the output bounds are derived by evaluating f at the input bound
// configuration that produces the output extremum:
//   non-decreasing in arg → input min produces output min, input max produces output max
//   non-increasing in arg → input max produces output min, input min produces output max
// Multi-arg monotonic functions (e.g., date_diff matches+inverts) are handled by picking
// each arg's bound independently for the min and max configurations.
//
// Foldable args are evaluated to their constant value once and reused for both bounds.
// NULL outputs cause the propagation to bail (handles e.g. year(infinity) = NULL for
// Tier-2 functions; the per-row execution path will produce the correct answer).
static unique_ptr<BaseStatistics> TryPropagateMonotonic(ClientContext &context, BoundFunctionExpression &func,
                                                        const vector<BaseStatistics> &stats) {
	if (func.function.GetStability() != FunctionStability::CONSISTENT) {
		return nullptr;
	}
	const auto &mono = func.function.GetMonotonicity();
	if (!mono.HasMonotonicArg()) {
		return nullptr;
	}
	if (!func.return_type.IsNumeric() && !func.return_type.IsTemporal()) {
		return nullptr;
	}

	vector<Value> min_config(func.children.size());
	vector<Value> max_config(func.children.size());
	for (idx_t i = 0; i < func.children.size(); i++) {
		if (func.children[i]->IsFoldable()) {
			Value v;
			if (!ExpressionExecutor::TryEvaluateScalar(context, *func.children[i], v)) {
				return nullptr;
			}
			min_config[i] = v;
			max_config[i] = v;
			continue;
		}
		auto direction = mono.GetOutputOrderForArg(i);
		if (direction == FunctionOutputOrder::UNSPECIFIED) {
			return nullptr;
		}
		if (stats[i].GetStatsType() != StatisticsType::NUMERIC_STATS) {
			return nullptr;
		}
		if (!NumericStats::HasMinMax(stats[i])) {
			return nullptr;
		}
		Value imin = NumericStats::Min(stats[i]);
		Value imax = NumericStats::Max(stats[i]);
		if (direction == FunctionOutputOrder::MATCHES_INPUT_ORDER) {
			min_config[i] = std::move(imin);
			max_config[i] = std::move(imax);
		} else {
			// INVERTS_INPUT_ORDER: larger input produces smaller output
			min_config[i] = std::move(imax);
			max_config[i] = std::move(imin);
		}
	}

	auto eval_at = [&](const vector<Value> &config) -> Value {
		auto func_copy = func.Copy();
		auto &fcopy = func_copy->Cast<BoundFunctionExpression>();
		for (idx_t i = 0; i < config.size(); i++) {
			fcopy.children[i] = make_uniq<BoundConstantExpression>(config[i]);
		}
		Value result;
		if (!ExpressionExecutor::TryEvaluateScalar(context, *func_copy, result)) {
			return Value();
		}
		return result;
	};

	Value out_min = eval_at(min_config);
	Value out_max = eval_at(max_config);
	if (out_min.IsNull() || out_max.IsNull()) {
		return nullptr;
	}

	auto result = NumericStats::CreateEmpty(func.return_type);
	NumericStats::SetMin(result, out_min);
	NumericStats::SetMax(result, out_max);
	return result.ToUnique();
}

unique_ptr<BaseStatistics> StatisticsPropagator::PropagateExpression(BoundFunctionExpression &func,
                                                                     unique_ptr<Expression> &expr_ptr) {
	vector<BaseStatistics> stats;
	stats.reserve(func.children.size());
	for (idx_t i = 0; i < func.children.size(); i++) {
		auto stat = PropagateExpression(func.children[i]);
		if (!stat) {
			stats.push_back(BaseStatistics::CreateUnknown(func.children[i]->return_type));
		} else {
			stats.push_back(stat->Copy());
		}
	}
	// First try the per-function stats callback (existing path).
	if (func.function.HasStatisticsCallback()) {
		FunctionStatisticsInput input(func, func.bind_info.get(), stats, &expr_ptr);
		auto result = func.function.GetStatisticsCallback()(context, input);
		if (result) {
			return result;
		}
	}
	// Fall back to monotonicity-based range propagation.
	return TryPropagateMonotonic(context, func, stats);
}

} // namespace duckdb
