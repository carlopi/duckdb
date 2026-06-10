# The fork contributes only the parsing machinery (namespace duckdb_fork):
# parser.cpp + the PEG subtree. AST classes (statement/, expression/, ...) are
# NOT compiled here - they resolve to the host DuckDB's namespace duckdb symbols.

# Make this tree's headers win over the host DuckDB's for the renamed files.
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Start from a clean object list: only fork objects may flow to the parent scope.
set(ALL_OBJECT_FILES)

add_subdirectory(peg)

add_library_unity(duckdb_fork_parser OBJECT parser.cpp)
set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_fork_parser>
    PARENT_SCOPE)
