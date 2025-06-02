add_extension_definitions()

add_definitions(-DDUCKDB_ROOT_DIRECTORY="${PROJECT_SOURCE_DIR}"
                -DDUCKDB_BUILD_DIRECTORY="${PROJECT_BINARY_DIR}")

set(SQLITE_TEST_RUNNER_SOURCES
    result_helper.cpp sqllogic_command.cpp sqllogic_test_runner.cpp
    sqllogic_parser.cpp test_sqllogictest.cpp sqllogic_test_logger.cpp)

add_library_unity(test_sqlite OBJECT ${SQLITE_TEST_RUNNER_SOURCES})
set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:test_sqlite>
    PARENT_SCOPE)

include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG master # Can be a tag (yaml-cpp-x.x.x), a commit hash, or a branch
                 # name (master)
)
FetchContent_MakeAvailable(yaml-cpp)

get_property(
  YAML_INCLUDE_PROP
  TARGET yaml-cpp
  PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "YAML Include folder = ${YAML_INCLUDE_PROP}")
target_include_directories(test_sqlite PRIVATE "${YAML_INCLUDE_PROP}")
