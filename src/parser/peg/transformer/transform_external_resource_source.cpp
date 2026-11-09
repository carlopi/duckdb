#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/peg/ast/generic_copy_option.hpp"
#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/external_resource_family.hpp"
#include "duckdb/parser/parsed_data/external_resource_options.hpp"
#include "duckdb/parser/statement/attach_statement.hpp"
#include "duckdb/parser/statement/connect_statement.hpp"

namespace duckdb {

// Both statements produce a normal ATTACH/CONNECT carrying an ExternalResourceOptions. `NEW TEMPORARY`
// provisions a resource the statement owns; a bare `<name>` borrows a registered one.

//! NEW TEMPORARY EXTERNAL RESOURCE '<type>' [(create opts)] — provision a fresh resource. A family in place of
//! EXTERNAL folds into the type (see ExternalResourceFamily).
unique_ptr<ExternalResourceOptions> PEGTransformerFactory::TransformExternalResourceCreateClause(
    PEGTransformer &transformer, const Identifier &external_resource_family, const string &string_literal,
    const optional<vector<GenericCopyOption>> &external_resource_creation_options) {
	auto result = make_uniq<ExternalResourceOptions>();
	result->provider =
	    ExternalResourceFamily::QualifyType(external_resource_family.GetIdentifierName(), string_literal);
	if (external_resource_creation_options) {
		CollectGenericOptions(*external_resource_creation_options, result->parsed_params, "EXTERNAL RESOURCE");
	}
	return result;
}

//! EXTERNAL RESOURCE <name> — reference an already-registered resource. A family in place of EXTERNAL is kept,
//! and the record is checked against it at bind.
unique_ptr<ExternalResourceOptions> PEGTransformerFactory::TransformExternalResourceReferenceClause(
    PEGTransformer &transformer, const Identifier &external_resource_family, const Identifier &col_id) {
	auto result = make_uniq<ExternalResourceOptions>();
	result->reference_name = col_id.GetIdentifierName();
	if (!ExternalResourceFamily::IsRoot(external_resource_family.GetIdentifierName())) {
		result->family = external_resource_family.GetIdentifierName();
	}
	return result;
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformAttachToExternalResource(
    PEGTransformer &transformer, unique_ptr<ExternalResourceOptions> external_resource_source,
    const Identifier &attach_alias, const optional<vector<GenericCopyOption>> &attach_options) {
	auto result = make_uniq<AttachStatement>();
	auto info = make_uniq<AttachInfo>();
	info->on_conflict = OnCreateConflict::ERROR_ON_CONFLICT;
	// The alias is mandatory in the grammar: it separates the create params from the attach options.
	info->name = Identifier(attach_alias);
	info->external_resource = std::move(external_resource_source);
	if (attach_options) {
		SplitGenericOptions(*attach_options, info->parsed_options, info->options, "ATTACH");
	}
	result->info = std::move(info);
	return std::move(result);
}

//! CONNECT has no alias separating its options from the recipe's. Borrowing takes no create params, so
//! a list there is the connection's; provisioning claims the first, and a second is refused.
unique_ptr<SQLStatement>
PEGTransformerFactory::TransformConnectToExternalResource(PEGTransformer &transformer,
                                                          unique_ptr<ExternalResourceOptions> external_resource_source,
                                                          const optional<vector<GenericCopyOption>> &attach_options) {
	auto result = make_uniq<ConnectStatement>();
	auto info = make_uniq<ConnectInfo>();
	if (attach_options && external_resource_source->reference_name.empty()) {
		throw ParserException("CONNECT TO NEW TEMPORARY EXTERNAL RESOURCE: the option list belongs to the resource, so "
		                      "the connection cannot take one of its own. Use `ATTACH TO NEW TEMPORARY EXTERNAL "
		                      "RESOURCE ... AS <name> (options); CONNECT <name>;` instead");
	}
	info->external_resource = std::move(external_resource_source);
	if (attach_options) {
		SplitGenericOptions(*attach_options, info->parsed_options, info->options, "CONNECT");
	}
	result->info = std::move(info);
	return std::move(result);
}

} // namespace duckdb
