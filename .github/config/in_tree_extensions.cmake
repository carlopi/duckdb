#
# This is the DuckDB in-tree extension config as it will run on the CI
#
# to build duckdb with this configuration run:
#   EXTENSION_CONFIGS=.github/config/in_tree_extensions.cmake make
#

duckdb_extension_load(autocomplete LOAD_TESTS)
duckdb_extension_load(core_functions LOAD_TESTS)
duckdb_extension_load(httpfs LOAD_TESTS)
duckdb_extension_load(icu LOAD_TESTS)
duckdb_extension_load(json LOAD_TESTS)
duckdb_extension_load(parquet LOAD_TESTS)
duckdb_extension_load(tpcds LOAD_TESTS)
duckdb_extension_load(tpch LOAD_TESTS)

# Test extension for the upcoming C CAPI extensions
duckdb_extension_load(demo_capi DONT_LINK)
