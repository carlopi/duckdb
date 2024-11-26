PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=httpfs
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile

configure_ci:
	cd duckdb && BUILD_ALL_EXT=1 make extension_configuration && cp build/extension_configuration/vcpkg.json ../.

test_release:
	python3 duckdb/scripts/run_tests_one_by_one.py ./build/release/test/unittest
