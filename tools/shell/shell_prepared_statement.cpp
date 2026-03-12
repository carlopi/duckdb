#include "shell_prepared_statement_local.hpp"
#include "shell_query_result_local.hpp"

namespace duckdb_shell {
using duckdb::make_uniq;

ShellPreparedStatementLocal::ShellPreparedStatementLocal(duckdb::unique_ptr<duckdb::PreparedStatement> statement)
    : statement(std::move(statement)) {
}

ShellPreparedStatementLocal::~ShellPreparedStatementLocal() {
}

bool ShellPreparedStatementLocal::HasError() const {
	return statement->HasError();
}

const duckdb::string &ShellPreparedStatementLocal::GetError() const {
	return statement->GetError();
}

unique_ptr<ShellQueryResult> ShellPreparedStatementLocal::Execute(duckdb::vector<duckdb::Value> &values) {
	return make_uniq<ShellQueryResultLocal>(statement->Execute(values));
}

} // namespace duckdb_shell
