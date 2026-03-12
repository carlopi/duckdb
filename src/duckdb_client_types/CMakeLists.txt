# Minimal type library for the wire-mode shell client.
# Contains only the types, serialization, parsing, and rendering code needed
# to parse, deserialize, and display query results — no catalog, planner,
# executor, optimizer, or storage engine.

# ── Parser (full — parsing is client-side) ──────────────────────────
file(GLOB_RECURSE PARSER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/parser/*.cpp)

set(CLIENT_TYPES_SOURCES
    ${PARSER_SOURCES}

    # Infrastructure
    ${CMAKE_SOURCE_DIR}/src/common/allocator.cpp
    ${CMAKE_SOURCE_DIR}/src/common/exception.cpp
    ${CMAKE_SOURCE_DIR}/src/common/exception_format_value.cpp
    ${CMAKE_SOURCE_DIR}/src/common/error_data.cpp
    ${CMAKE_SOURCE_DIR}/src/common/stacktrace.cpp
    ${CMAKE_SOURCE_DIR}/src/common/complex_json.cpp
    ${CMAKE_SOURCE_DIR}/src/common/random_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/common/printer.cpp
    ${CMAKE_SOURCE_DIR}/src/common/string_util.cpp

    # Progress bar display
    ${CMAKE_SOURCE_DIR}/src/common/progress_bar/terminal_progress_bar_display.cpp
    ${CMAKE_SOURCE_DIR}/src/common/progress_bar/unscented_kalman_filter.cpp

    # Query result base
    ${CMAKE_SOURCE_DIR}/src/main/query_result.cpp

    # Core type system (LogicalType, extra_type_info)
    ${CMAKE_SOURCE_DIR}/src/common/types.cpp
    ${CMAKE_SOURCE_DIR}/src/common/extra_type_info.cpp

    # Scalar types
    ${CMAKE_SOURCE_DIR}/src/common/types/date.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/time.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/timestamp.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/interval.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/decimal.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/hugeint.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/uhugeint.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/uuid.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/blob.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/bit.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/bignum.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/hash.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/string_heap.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/string_type.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/cast_helpers.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/geometry.cpp

    # Cast / string conversion operators
    ${CMAKE_SOURCE_DIR}/src/common/operator/cast_operators.cpp
    ${CMAKE_SOURCE_DIR}/src/common/operator/convert_to_string.cpp
    ${CMAKE_SOURCE_DIR}/src/common/operator/string_cast.cpp

    # Variant support
    ${CMAKE_SOURCE_DIR}/src/common/types/variant/variant.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/variant/variant_value.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/variant/variant_value_convert.cpp
    ${CMAKE_SOURCE_DIR}/src/function/scalar/variant/variant_utils.cpp

    # Vector infrastructure
    ${CMAKE_SOURCE_DIR}/src/common/types/selection_vector.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/validity_mask.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/vector_cache.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/vector_constants.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/vector.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/vector_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/data_chunk.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/value.cpp

    # Vector operations
    ${CMAKE_SOURCE_DIR}/src/common/vector_operations/null_operations.cpp
    ${CMAKE_SOURCE_DIR}/src/common/vector_operations/vector_storage.cpp
    ${CMAKE_SOURCE_DIR}/src/common/vector_operations/generators.cpp
    ${CMAKE_SOURCE_DIR}/src/common/vector_operations/vector_hash.cpp
    ${CMAKE_SOURCE_DIR}/src/common/vector_operations/vector_cast.cpp
    ${CMAKE_SOURCE_DIR}/src/common/vector_operations/vector_copy.cpp

    # Serialization
    ${CMAKE_SOURCE_DIR}/src/common/serializer/binary_serializer.cpp
    ${CMAKE_SOURCE_DIR}/src/common/serializer/binary_deserializer.cpp
    ${CMAKE_SOURCE_DIR}/src/common/serializer/serializer.cpp
    ${CMAKE_SOURCE_DIR}/src/common/serializer/memory_stream.cpp

    # Column data
    ${CMAKE_SOURCE_DIR}/src/common/types/column/column_data_collection_segment.cpp
    ${CMAKE_SOURCE_DIR}/src/common/types/column/column_data_collection.cpp

    # Storage (minimal — only what Vector/StringHeap need)
    ${CMAKE_SOURCE_DIR}/src/storage/buffer/buffer_handle.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/arena_allocator.cpp

    # Rendering
    ${CMAKE_SOURCE_DIR}/src/common/box_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/common/box_renderer_context.cpp

    # Exceptions
    ${CMAKE_SOURCE_DIR}/src/common/exception/conversion_exception.cpp
    ${CMAKE_SOURCE_DIR}/src/common/exception/parser_exception.cpp

    # Enums & utilities
    ${CMAKE_SOURCE_DIR}/src/common/enum_util.cpp
    ${CMAKE_SOURCE_DIR}/src/common/constants.cpp
    ${CMAKE_SOURCE_DIR}/src/common/serialization_compatibility.cpp
    ${CMAKE_SOURCE_DIR}/src/common/enums/date_part_specifier.cpp

    # Value operations
    ${CMAKE_SOURCE_DIR}/src/common/value_operations/comparison_operations.cpp

    # File system (needed by shell for home directory, path joining)
    ${CMAKE_SOURCE_DIR}/src/common/file_system.cpp
    ${CMAKE_SOURCE_DIR}/src/common/local_file_system.cpp

    # Cast infrastructure — NOT included (would pull all cast implementations)
    # ${CMAKE_SOURCE_DIR}/src/function/cast/cast_function_set.cpp
    # ${CMAKE_SOURCE_DIR}/src/function/cast/default_casts.cpp

    # Logging
    ${CMAKE_SOURCE_DIR}/src/logging/log_storage.cpp

    # Error manager
    ${CMAKE_SOURCE_DIR}/src/main/error_manager.cpp

    # Extension callback manager (parser needs it)
    ${CMAKE_SOURCE_DIR}/src/main/extension_callback_manager.cpp

    # Planner binding alias (referenced by parser)
    ${CMAKE_SOURCE_DIR}/src/planner/binding_alias.cpp

    # FSST decompression
    ${CMAKE_SOURCE_DIR}/src/storage/compression/fsst.cpp

    # Version stubs (wire mode — real values come from server)
    ${CMAKE_SOURCE_DIR}/src/duckdb_client_types/version_stubs.cpp
)

add_library(duckdb_client_types STATIC ${CLIENT_TYPES_SOURCES})

target_include_directories(duckdb_client_types PUBLIC
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/third_party/fsst
    ${CMAKE_SOURCE_DIR}/third_party/fmt/include
    ${CMAKE_SOURCE_DIR}/third_party/utf8proc/include
    ${CMAKE_SOURCE_DIR}/third_party/re2
    ${CMAKE_SOURCE_DIR}/third_party/hyperloglog
    ${CMAKE_SOURCE_DIR}/third_party/fastpforlib
    ${CMAKE_SOURCE_DIR}/third_party/skiplist
    ${CMAKE_SOURCE_DIR}/third_party/fast_float
    ${CMAKE_SOURCE_DIR}/third_party/miniz
    ${CMAKE_SOURCE_DIR}/third_party/concurrentqueue
    ${CMAKE_SOURCE_DIR}/third_party/pcg
    ${CMAKE_SOURCE_DIR}/third_party/tdigest
    ${CMAKE_SOURCE_DIR}/third_party/mbedtls/include
    ${CMAKE_SOURCE_DIR}/third_party/jaro_winkler
    ${CMAKE_SOURCE_DIR}/third_party/yyjson/include
    ${CMAKE_SOURCE_DIR}/third_party/zstd/include
    ${CMAKE_SOURCE_DIR}/third_party/libpg_query/include
)

target_link_libraries(duckdb_client_types
    duckdb_fmt
    duckdb_utf8proc
    duckdb_fsst
    duckdb_yyjson
    duckdb_pg_query
    duckdb_re2
    duckdb_miniz
)
