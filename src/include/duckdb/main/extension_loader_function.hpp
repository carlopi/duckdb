#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/enums/date_part_specifier.hpp"
#include "duckdb/common/types/date.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/common/vector_operations/binary_executor.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"

namespace duckdb {

static void FuncNop(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.GetTypes().size() == 1);
	result.Reinterpret(input.data[0]);
}

unique_ptr<FunctionData> BindNop(ClientContext &context, ScalarFunction &bound_function,
                                    vector<unique_ptr<Expression>> &arguments) {

	bound_function.return_type = arguments[0]->return_type;

	return nullptr;
}

void RegisterLoadExtensionFunction(DatabaseInstance &db, const string &function_name) {
	ScalarFunctionSet set(function_name);
	set.AddFunction(ScalarFunction({LogicalType::ANY}, LogicalType::ANY, FuncNop, BindNop));
	ExtensionUtil::RegisterFunction(db, set);
}

} // namespace duckdb
