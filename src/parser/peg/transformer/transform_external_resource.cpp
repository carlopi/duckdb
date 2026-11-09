#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/star_expression.hpp"
#include "duckdb/parser/external_resource_family.hpp"
#include "duckdb/parser/peg/ast/generic_copy_option.hpp"
#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/statement/external_resource_statement.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"

namespace duckdb {

//! Fold `(k v, ...)` create params onto the statement; they are resolved at bind time.
static void ApplyOptions(const optional<vector<GenericCopyOption>> &options, ExternalResourceStatement &stmt) {
	if (!options) {
		return;
	}
	PEGTransformerFactory::CollectGenericOptions(*options, stmt.options, "EXTERNAL RESOURCE");
}

// The word before RESOURCE folds into the type: `CREATE AWS RESOURCE 's3tables'` creates an 'aws:s3tables'.
unique_ptr<SQLStatement> PEGTransformerFactory::TransformCreateExternalResourceStmt(
    PEGTransformer &transformer, const Identifier &external_resource_family, const string &string_literal,
    const optional<Identifier> &attach_alias,
    const optional<vector<GenericCopyOption>> &external_resource_creation_options) {
	auto result = make_uniq<ExternalResourceStatement>(ExternalResourceOperation::CREATE);
	result->type = ExternalResourceFamily::QualifyType(external_resource_family.GetIdentifierName(), string_literal);
	if (attach_alias) {
		result->name = Identifier(*attach_alias);
	}
	ApplyOptions(external_resource_creation_options, *result);
	return std::move(result);
}

unique_ptr<SQLStatement>
PEGTransformerFactory::TransformRegisterExternalResourceStmt(PEGTransformer &transformer,
                                                             const Identifier &external_resource_family,
                                                             const string &string_literal,
                                                             const optional<Identifier> &attach_alias,
                                                             unique_ptr<ParsedExpression> expression) {
	auto result = make_uniq<ExternalResourceStatement>(ExternalResourceOperation::REGISTER);
	result->type = ExternalResourceFamily::QualifyType(external_resource_family.GetIdentifierName(), string_literal);
	if (attach_alias) {
		result->name = Identifier(*attach_alias);
	}
	result->handle = std::move(expression);
	return std::move(result);
}

// DESTROY names a record, not a type, so the family stays on the statement and the record is checked against it.
unique_ptr<SQLStatement>
PEGTransformerFactory::TransformDestroyExternalResourceStmt(PEGTransformer &transformer,
                                                            const Identifier &external_resource_family,
                                                            const Identifier &col_id) {
	auto result = make_uniq<ExternalResourceStatement>(ExternalResourceOperation::DESTROY);
	result->name = col_id;
	if (!ExternalResourceFamily::IsRoot(external_resource_family.GetIdentifierName())) {
		result->family = external_resource_family.GetIdentifierName();
	}
	return std::move(result);
}

//! A named argument of a table function call: the name goes on the argument and, for the binder, on the expression.
static void AddNamedArgument(vector<FunctionArgument> &children, const string &name,
                             unique_ptr<ParsedExpression> value) {
	value->SetAlias(Identifier(name));
	children.emplace_back(Identifier(name), std::move(value));
}

// SHOW [ALL] EXTERNAL RESOURCES [WITH TYPE t] is the query
// `SELECT * FROM duckdb_external_resources(discover := <all>, type := t)`, so it can be used wherever a query can,
// such as a subquery. A family in place of EXTERNAL is the type filter by itself (`SHOW ALL AWS RESOURCES` lists
// the 'aws:...' types), and the type after WITH TYPE is spelled relative to it.
unique_ptr<QueryNode>
PEGTransformerFactory::TransformShowExternalResources(PEGTransformer &transformer, const ShowType &show_rule,
                                                      const bool &has_result, const Identifier &external_resource_family,
                                                      optional<Identifier> show_external_resources_type) {
	vector<FunctionArgument> children;
	if (has_result) {
		AddNamedArgument(children, "discover", ConstantExpression::Boolean(true));
	}
	auto &family = external_resource_family.GetIdentifierName();
	string type;
	if (show_external_resources_type) {
		type = ExternalResourceFamily::QualifyType(family, show_external_resources_type->GetIdentifierName());
	} else if (!ExternalResourceFamily::IsRoot(family)) {
		type = family;
	}
	if (!type.empty()) {
		AddNamedArgument(children, "type", ConstantExpression::String(type));
	}
	auto table_function = make_uniq<TableFunctionRef>();
	table_function->function = make_uniq<FunctionExpression>("duckdb_external_resources", std::move(children));

	auto select_node = make_uniq<SelectNode>();
	select_node->select_list.push_back(make_uniq<StarExpression>());
	select_node->from_table = std::move(table_function);
	return std::move(select_node);
}

} // namespace duckdb
