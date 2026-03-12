#include "shell_prepared_statement.hpp"

namespace duckdb_shell {

ShellPreparedStatement::ShellPreparedStatement(duckdb::unique_ptr<duckdb::PreparedStatement> statement)
    : statement(std::move(statement)) {
}

ShellPreparedStatement::~ShellPreparedStatement() {
}

bool ShellPreparedStatement::HasError() const {
	return statement->HasError();
}

const duckdb::string &ShellPreparedStatement::GetError() const {
	return statement->GetError();
}

unique_ptr<ShellQueryResult> ShellPreparedStatement::Execute(duckdb::vector<duckdb::Value> &values) {
	return make_uniq<ShellQueryResult>(statement->Execute(values));
}

} // namespace duckdb_shell
