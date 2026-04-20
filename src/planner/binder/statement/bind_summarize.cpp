#include "duckdb/planner/binder.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/expression/star_expression.hpp"
#include "duckdb/parser/tableref/showref.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "duckdb/parser/tableref/subqueryref.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {

BoundStatement Binder::BindSummarize(ShowRef &ref) {
	// Build the source query SQL string
	string source_sql;
	if (ref.query) {
		source_sql = ref.query->ToString();
	} else {
		auto table_name = QualifiedName::Parse(ref.table_name);
		auto node = make_uniq<SelectNode>();
		node->select_list.push_back(make_uniq<StarExpression>());
		auto basetableref = make_uniq<BaseTableRef>();
		basetableref->catalog_name = table_name.catalog;
		basetableref->schema_name = table_name.schema;
		basetableref->table_name = table_name.name;
		node->from_table = std::move(basetableref);
		source_sql = node->ToString();
	}

	// Build the full summarize query using a single-pass aggregate with COLUMNS(*),
	// UNPIVOT to transpose columns to rows, and a reshape step.
	// This avoids a preliminary bind (which would consume non-seekable streams like stdin)
	// and pushes all type-conditional logic into SQL.
	// All quantiles are computed as DOUBLE for performance; integer types are cast back
	// to BIGINT in the final reshape (which operates on N rows, not the full dataset).
	// TODO: temporal types (TIMESTAMP, DATE, TIME, TIMETZ) produce NULL for avg/quantiles
	// because TRY_CAST(COLUMNS(*) AS DOUBLE) fails for them. Fixing this requires
	// epoch_us conversion before aggregation and type-conditional conversion back,
	// which may show the limits of the pure-SQL COLUMNS(*) approach.
	string summarize_sql = R"(
WITH __summarize_src AS MATERIALIZED ()" +
	                       source_sql + R"(),
__summarize_agg AS (
  SELECT
    MIN(COLUMNS(*))::VARCHAR,
    MAX(COLUMNS(*))::VARCHAR,
    APPROX_COUNT_DISTINCT(COLUMNS(*))::VARCHAR,
    COALESCE(TRY_CAST(AVG(TRY_CAST(COLUMNS(*) AS DOUBLE)) AS VARCHAR), '__SUMMARIZE_NULL__'),
    COALESCE(TRY_CAST(STDDEV(TRY_CAST(COLUMNS(*) AS DOUBLE)) AS VARCHAR), '__SUMMARIZE_NULL__'),
    COALESCE(TRY_CAST(APPROX_QUANTILE(TRY_CAST(COLUMNS(*) AS DOUBLE), 0.25) AS VARCHAR), '__SUMMARIZE_NULL__'),
    COALESCE(TRY_CAST(APPROX_QUANTILE(TRY_CAST(COLUMNS(*) AS DOUBLE), 0.50) AS VARCHAR), '__SUMMARIZE_NULL__'),
    COALESCE(TRY_CAST(APPROX_QUANTILE(TRY_CAST(COLUMNS(*) AS DOUBLE), 0.75) AS VARCHAR), '__SUMMARIZE_NULL__'),
    COALESCE(CASE WHEN COUNT(*) > 0
                  THEN ROUND((1.0 - COUNT(COLUMNS(*))::DOUBLE / COUNT(*)::DOUBLE) * 100, 2)::VARCHAR
             END, '__SUMMARIZE_NULL__'),
    ANY_VALUE(typeof(COLUMNS(*))),
    COUNT(*)::BIGINT AS __summarize_count_star
  FROM __summarize_src
),
__summarize_flat AS (
  SELECT name, value, (row_number() OVER () - 1) AS rn
  FROM (UNPIVOT (SELECT * EXCLUDE (__summarize_count_star) FROM __summarize_agg) ON COLUMNS(*))
),
__summarize_ncols AS (
  SELECT COUNT(DISTINCT regexp_replace(name, '_\d+$', '')) AS n FROM __summarize_flat
),
__summarize_int_types AS (
  SELECT unnest(['TINYINT','SMALLINT','INTEGER','BIGINT',
                 'UTINYINT','USMALLINT','UINTEGER','UBIGINT',
                 'HUGEINT','UHUGEINT']) AS t
)
SELECT
  regexp_replace(name, '_\d+$', '') AS column_name,
  NULLIF(list_extract(vals, 10), '__SUMMARIZE_NULL__') AS column_type,
  NULLIF(list_extract(vals, 1), '__SUMMARIZE_NULL__') AS min,
  NULLIF(list_extract(vals, 2), '__SUMMARIZE_NULL__') AS max,
  TRY_CAST(list_extract(vals, 3) AS BIGINT) AS approx_unique,
  NULLIF(list_extract(vals, 4), '__SUMMARIZE_NULL__') AS avg,
  NULLIF(list_extract(vals, 5), '__SUMMARIZE_NULL__') AS std,
  CASE WHEN NULLIF(list_extract(vals, 10), '__SUMMARIZE_NULL__') IN (SELECT t FROM __summarize_int_types)
       THEN TRY_CAST(TRY_CAST(NULLIF(list_extract(vals, 6), '__SUMMARIZE_NULL__') AS BIGINT) AS VARCHAR)
       ELSE NULLIF(list_extract(vals, 6), '__SUMMARIZE_NULL__') END AS q25,
  CASE WHEN NULLIF(list_extract(vals, 10), '__SUMMARIZE_NULL__') IN (SELECT t FROM __summarize_int_types)
       THEN TRY_CAST(TRY_CAST(NULLIF(list_extract(vals, 7), '__SUMMARIZE_NULL__') AS BIGINT) AS VARCHAR)
       ELSE NULLIF(list_extract(vals, 7), '__SUMMARIZE_NULL__') END AS q50,
  CASE WHEN NULLIF(list_extract(vals, 10), '__SUMMARIZE_NULL__') IN (SELECT t FROM __summarize_int_types)
       THEN TRY_CAST(TRY_CAST(NULLIF(list_extract(vals, 8), '__SUMMARIZE_NULL__') AS BIGINT) AS VARCHAR)
       ELSE NULLIF(list_extract(vals, 8), '__SUMMARIZE_NULL__') END AS q75,
  (SELECT __summarize_count_star FROM __summarize_agg) AS count,
  TRY_CAST(NULLIF(list_extract(vals, 9), '__SUMMARIZE_NULL__') AS DECIMAL(9, 2)) AS null_percentage
FROM (
  SELECT min(name) AS name, list(value ORDER BY rn) AS vals, min(rn) AS col_order
  FROM __summarize_flat
  GROUP BY rn % (SELECT n FROM __summarize_ncols)
  ORDER BY col_order
)
)";

	Parser parser(context.GetParserOptions());
	parser.ParseQuery(summarize_sql);
	D_ASSERT(parser.statements.size() == 1);

	auto select_stmt = unique_ptr_cast<SQLStatement, SelectStatement>(std::move(parser.statements[0]));
	auto subquery = make_uniq<SubqueryRef>(std::move(select_stmt));
	return Bind(*subquery);
}

} // namespace duckdb
