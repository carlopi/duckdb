#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/statement/explain_statement.hpp"
#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"

namespace duckdb_fork {
using namespace duckdb;
/*
ExplainFormat ParseExplainFormat(const Value &val) {
	if (val.type().id() != LogicalTypeId::VARCHAR) {
		throw InvalidInputException("Expected a string as argument to FORMAT");
	}
	auto format_val = val.GetValue<string>();
	case_insensitive_map_t<ExplainFormat> format_mapping {
	    {"default", ExplainFormat::DEFAULT}, {"text", ExplainFormat::TEXT},         {"json", ExplainFormat::JSON},
	    {"html", ExplainFormat::HTML},       {"graphviz", ExplainFormat::GRAPHVIZ}, {"yaml", ExplainFormat::YAML},
	    {"mermaid", ExplainFormat::MERMAID}};
	auto it = format_mapping.find(format_val);
	if (it != format_mapping.end()) {
		return it->second;
	}
	vector<string> options_list;
	for (auto &format : format_mapping) {
		options_list.push_back(format.first);
	}
	auto allowed_options = StringUtil::Join(options_list, ", ");
	throw InvalidInputException("\"%s\" is not a valid FORMAT argument, valid options are: %s", format_val,
	                            allowed_options);
}

*/
unique_ptr<SQLStatement>
PEGTransformerFactory::TransformExplainStatement(PEGTransformer &transformer, const bool &explain_analyze,
                                                 const vector<GenericCopyOption> &explain_option_list,
                                                 unique_ptr<SQLStatement> explainable_statements) {

	return nullptr;
}
bool PEGTransformerFactory::TransformExplainAnalyze(PEGTransformer &transformer) {
	return true;
}

unique_ptr<SQLStatement>
PEGTransformerFactory::TransformExplainSelectStatement(PEGTransformer &transformer,
                                                       unique_ptr<SelectStatement> select_statement_internal) {
	return std::move(select_statement_internal);
}

vector<GenericCopyOption>
PEGTransformerFactory::TransformExplainOptionList(PEGTransformer &transformer,
                                                  const vector<GenericCopyOption> &explain_option) {
	return explain_option;
}

GenericCopyOption PEGTransformerFactory::TransformExplainOption(PEGTransformer &transformer,
                                                                const Identifier &explain_option_name,
                                                                unique_ptr<ParsedExpression> expression) {
	GenericCopyOption copy_option;
	copy_option.name = Identifier(StringUtil::Lower(explain_option_name.GetIdentifierName()));
	if (!expression) {
		return copy_option;
	}
	if (expression->GetExpressionType() == ExpressionType::VALUE_CONSTANT) {
		copy_option.children.push_back(Value(expression->Cast<ConstantExpression>().GetValue()));
	} else if (expression->GetExpressionType() == ExpressionType::COLUMN_REF) {
		copy_option.children.push_back(Value(expression->Cast<ColumnRefExpression>().GetColumnName()));
	} else {
		copy_option.expression = std::move(expression);
	}
	return copy_option;
}

} // namespace duckdb_fork
