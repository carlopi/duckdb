#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/tableref/emptytableref.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformAssertStatement(
    PEGTransformer &transformer, CommonTableExpressionMap with_clause, unique_ptr<ParsedExpression> aliased_expression,
    unique_ptr<TableRef> from_clause, unique_ptr<ParsedExpression> where_clause, GroupByNode group_by_clause,
    unique_ptr<ParsedExpression> having_clause) {
	// ASSERT <expr> [[AS] <alias>] [FROM ...] desugars into
	// SELECT assert_true(<expr>, '<alias, or the expression text>') [FROM ...]
	// assert_true returns NULL when the condition holds and throws 'Assertion: <message>' otherwise
	// (FALSE and NULL conditions both throw)
	auto &alias = aliased_expression->GetAlias();
	auto message = alias.empty() ? aliased_expression->ToString() : alias.GetIdentifierName();
	aliased_expression->ClearAlias();

	vector<unique_ptr<ParsedExpression>> arguments;
	arguments.push_back(std::move(aliased_expression));
	arguments.push_back(make_uniq<ConstantExpression>(Value(message)));

	auto node = make_uniq<SelectNode>();
	node->select_list.push_back(make_uniq<FunctionExpression>(Identifier("assert_true"), std::move(arguments)));
	node->from_table = from_clause ? std::move(from_clause) : make_uniq_base<TableRef, EmptyTableRef>();
	node->where_clause = std::move(where_clause);
	node->groups = std::move(group_by_clause);
	node->having = std::move(having_clause);
	node->cte_map = std::move(with_clause);

	auto result = make_uniq<SelectStatement>();
	result->node = std::move(node);
	return std::move(result);
}

} // namespace duckdb
