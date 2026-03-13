include_directories(include)

# linenoise sets SHELL_SOURCES in parent scope via
# $<TARGET_OBJECTS:duckdb_linenoise>
set(SHELL_SOURCES "")
add_subdirectory(linenoise)
set(SHELL_LINENOISE_OBJECTS ${SHELL_SOURCES})
add_definitions(-DHAVE_LINENOISE=1)
include_directories(../../third_party/utf8proc/include)
include_directories(linenoise/include)
add_definitions(-DSQLITE_SHELL_IS_UTF8)
add_definitions(-DUSE_DUCKDB_SHELL_WRAPPER)

include_directories(../../third_party/utf8proc/include)

# Sources shared by both local and wire mode
set(SHELL_COMMON_SOURCES
    shell_column_data_collection.cpp
    shell_db_config.cpp
    shell_command_line_option.cpp
    shell.cpp
    shell_helpers.cpp
    shell_metadata_command.cpp
    shell_prompt.cpp
    shell_renderer.cpp
    shell_highlight.cpp
    shell_progress_bar.cpp
    shell_render_table_metadata.cpp
    shell_windows.cpp
    wire_serialization.cpp
    wire_transport.cpp)

# Sources that need the engine (Local implementations)
set(SHELL_LOCAL_SOURCES
    logical_type_properties.cpp
    shell_connection.cpp
    shell_prepared_statement.cpp
    shell_duckdb.cpp
    shell_extension.cpp
    shell_query_result.cpp
    shell_wired.cpp
    wire_transport_mock.cpp)

# Sources for wire mode (Wired implementations, no engine)
set(SHELL_WIRED_SOURCES shell_wired.cpp)

# Sources for wire test mode (Wired + MockTransportLayer backed by real engine)
set(SHELL_WIRE_TEST_SOURCES logical_type_properties.cpp shell_wired.cpp
                            wire_transport_mock.cpp)

if(DUCKDB_SHELL_WIRE_MODE)
  message("Building shell in WIRE MODE (no duckdb_static)")
  set(SHELL_SOURCES ${SHELL_LINENOISE_OBJECTS} ${SHELL_COMMON_SOURCES}
                    ${SHELL_WIRED_SOURCES})
elseif(DUCKDB_SHELL_WIRE_TEST)
  message(
    "Building shell in WIRE TEST MODE (wired path + MockTransportLayer + duckdb_static)"
  )
  set(SHELL_SOURCES ${SHELL_LINENOISE_OBJECTS} ${SHELL_COMMON_SOURCES}
                    ${SHELL_WIRE_TEST_SOURCES})
else()
  if(DUCKDB_EXTENSION_AUTOCOMPLETE_SHOULD_LINK)
    include_directories(../../extension/autocomplete/include)
    set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES}
        ../../extension/autocomplete/autocomplete_extension.cpp)
    add_definitions(-DSHELL_INLINE_AUTOCOMPLETE)
  endif()
  set(SHELL_SOURCES ${SHELL_LINENOISE_OBJECTS} ${SHELL_COMMON_SOURCES}
                    ${SHELL_LOCAL_SOURCES})
endif()

add_executable(shell ${SHELL_SOURCES})

if(DUCKDB_SHELL_WIRE_MODE)
  target_compile_definitions(shell PRIVATE DUCKDB_SHELL_WIRE_MODE)
  add_subdirectory(${CMAKE_SOURCE_DIR}/src/duckdb_client_types
                   ${CMAKE_BINARY_DIR}/duckdb_client_types)
  target_link_libraries(shell duckdb_client_types ${DUCKDB_EXTRA_LINK_FLAGS})
elseif(DUCKDB_SHELL_WIRE_TEST)
  target_compile_definitions(shell PRIVATE DUCKDB_SHELL_WIRE_TEST)
  target_link_libraries(shell duckdb_static ${DUCKDB_EXTRA_LINK_FLAGS})
  link_extension_libraries(shell "")
  if(NOT AMALGAMATION_BUILD AND NOT WIN32)
    target_link_libraries(shell duckdb_utf8proc)
  endif()
else()
  target_link_libraries(shell duckdb_static ${DUCKDB_EXTRA_LINK_FLAGS})
  link_extension_libraries(shell "")
  if(NOT AMALGAMATION_BUILD AND NOT WIN32)
    target_link_libraries(shell duckdb_utf8proc)
  endif()
endif()

link_threads(shell "")
if(STATIC_LIBCPP)
  message("Statically linking CLI")
  target_link_libraries(shell -static-libstdc++ -static-libgcc)
endif()

function(ensure_variable_is_number INPUT_VERSION OUT_RESULT)
  if(NOT "${${INPUT_VERSION}}" MATCHES "^[0-9]+$")
    message(
      WARNING
        "VERSION PARAMETER ${INPUT_VERSION} \"${${INPUT_VERSION}}\" IS NOT A NUMBER - SETTING TO 0"
    )
    set(${OUT_RESULT}
        0
        PARENT_SCOPE)
  else()
    set(${OUT_RESULT}
        ${${INPUT_VERSION}}
        PARENT_SCOPE)
  endif()
endfunction()

if(WIN32 AND NOT MINGW)
  string(TIMESTAMP DUCKDB_COPYRIGHT_YEAR "%Y")
  ensure_variable_is_number(DUCKDB_MAJOR_VERSION RC_MAJOR_VERSION)
  ensure_variable_is_number(DUCKDB_MINOR_VERSION RC_MINOR_VERSION)
  ensure_variable_is_number(DUCKDB_PATCH_VERSION RC_PATCH_VERSION)
  ensure_variable_is_number(DUCKDB_DEV_ITERATION RC_DEV_ITERATION)

  set(CMAKE_RC_FLAGS
      "${CMAKE_RC_FLAGS} -D DUCKDB_VERSION=\"${DUCKDB_VERSION}\"")
  set(CMAKE_RC_FLAGS
      "${CMAKE_RC_FLAGS} -D DUCKDB_MAJOR_VERSION=\"${RC_MAJOR_VERSION}\"")
  set(CMAKE_RC_FLAGS
      "${CMAKE_RC_FLAGS} -D DUCKDB_MINOR_VERSION=\"${RC_MINOR_VERSION}\"")
  set(CMAKE_RC_FLAGS
      "${CMAKE_RC_FLAGS} -D DUCKDB_PATCH_VERSION=\"${RC_PATCH_VERSION}\"")
  set(CMAKE_RC_FLAGS
      "${CMAKE_RC_FLAGS} -D DUCKDB_DEV_ITERATION=\"${RC_DEV_ITERATION}\"")
  set(CMAKE_RC_FLAGS
      "${CMAKE_RC_FLAGS} -D DUCKDB_COPYRIGHT_YEAR=\"${DUCKDB_COPYRIGHT_YEAR}\"")
  target_sources(shell PRIVATE rc/duckdb.rc)
endif()

set_target_properties(shell PROPERTIES OUTPUT_NAME duckdb)
set_target_properties(shell PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${PROJECT_BINARY_DIR})

install(TARGETS shell RUNTIME DESTINATION "${INSTALL_BIN_DIR}")
