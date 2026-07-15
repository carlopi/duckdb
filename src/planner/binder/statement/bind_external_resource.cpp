#include "duckdb/parser/statement/external_resource_statement.hpp"
#include "duckdb/planner/binder.hpp"

namespace duckdb {

BoundStatement Binder::Bind(ExternalResourceStatement &stmt) {
	// Parser scaffolding only: the CREATE / ADOPT / DESTROY / SHOW EXTERNAL RESOURCE verbs parse but are
	// not yet executed.
	throw NotImplementedException("EXTERNAL RESOURCE statements are not yet implemented");
}

} // namespace duckdb
