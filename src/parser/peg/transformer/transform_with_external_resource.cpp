#include "duckdb/parser/peg/ast/generic_copy_option.hpp"
#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/parsed_data/external_resource_options.hpp"
#include "duckdb/parser/statement/attach_statement.hpp"
#include "duckdb/parser/statement/connect_statement.hpp"

namespace duckdb {

// `WITH EXTERNAL RESOURCE '<type>' [AS r] [(params)] (ATTACH|CONNECT) [AS y] [(opts)]` produces a normal
// ATTACH/CONNECT carrying an ExternalResourceOptions (the resource to provision before attaching). The
// provider (type) is kept separate from the create params. The trailing attach-opts flow onto the info
// as ordinary options.
static unique_ptr<ExternalResourceOptions> BuildExternalResource(unique_ptr<ParsedExpression> type_expr,
                                                                 const optional<Identifier> &alias,
                                                                 const optional<vector<GenericCopyOption>> &params) {
	auto result = make_uniq<ExternalResourceOptions>();
	result->parsed_type = std::move(type_expr);
	if (alias) {
		result->alias = Identifier(*alias);
	}
	if (params) {
		for (const auto &opt : *params) {
			result->parsed_params[opt.name.GetIdentifierName()] = opt.GetFirstChildOrExpression();
		}
	}
	return result;
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformWithExternalResourceAttach(
    PEGTransformer &transformer, unique_ptr<ParsedExpression> expression, const optional<Identifier> &attach_alias,
    const optional<vector<GenericCopyOption>> &attach_options, const optional<Identifier> &attach_alias_1,
    const optional<vector<GenericCopyOption>> &attach_options_1) {
	auto result = make_uniq<AttachStatement>();
	auto info = make_uniq<AttachInfo>();
	info->on_conflict = OnCreateConflict::ERROR_ON_CONFLICT;
	if (attach_alias_1) {
		info->name = Identifier(*attach_alias_1);
	}
	info->external_resource = BuildExternalResource(std::move(expression), attach_alias, attach_options);
	if (attach_options_1) {
		SplitGenericOptions(*attach_options_1, info->parsed_options, info->options, "ATTACH");
	}
	result->info = std::move(info);
	return std::move(result);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformWithExternalResourceConnect(
    PEGTransformer &transformer, unique_ptr<ParsedExpression> expression, const optional<Identifier> &attach_alias,
    const optional<vector<GenericCopyOption>> &attach_options, const optional<Identifier> &attach_alias_1,
    const optional<vector<GenericCopyOption>> &attach_options_1) {
	auto result = make_uniq<ConnectStatement>();
	auto info = make_uniq<ConnectInfo>();
	// The connection is ephemeral and hidden (physical_connect names it internally), so `AS y` has no
	// referent in the CONNECT model and is ignored.
	info->external_resource = BuildExternalResource(std::move(expression), attach_alias, attach_options);
	if (attach_options_1) {
		SplitGenericOptions(*attach_options_1, info->parsed_options, info->options, "CONNECT");
	}
	result->info = std::move(info);
	return std::move(result);
}

} // namespace duckdb
