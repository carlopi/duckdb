duckdb_extension_load(fts
        LOAD_TESTS
        DONT_LINK
        GIT_URL https://github.com/duckdb/duckdb-fts
        GIT_TAG 513280f7de1c5e32171076ce2b62d5200cf64621
        TEST_DIR test/sql
        APPLY_PATCHES
)
