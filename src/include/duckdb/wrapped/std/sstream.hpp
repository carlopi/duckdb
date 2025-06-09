#pragma once

#include <sstream>

#define DUCKDB_WRAP_STD

namespace duckdb_wrapped {
namespace std {
using ::std::stringstream;
using ::std::wstringstream;
using ::std::basic_stringstream;
} // namespace std
} // namespace duckdb_wrapped
