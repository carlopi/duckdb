#include "logical_type_properties.hpp"
#include "duckdb/common/types.hpp"

namespace duckdb_shell {

LogicalTypeProperties LogicalTypeProperties::FromLogicalType(const duckdb::LogicalType &type) {
	LogicalTypeProperties props;
	props.name = type.ToString();
	props.is_numeric = type.IsNumeric();
	props.is_nested = type.IsNested();
	props.is_json = type.IsJSONType();
	props.is_boolean = (type.id() == duckdb::LogicalTypeId::BOOLEAN);
	return props;
}

} // namespace duckdb_shell
