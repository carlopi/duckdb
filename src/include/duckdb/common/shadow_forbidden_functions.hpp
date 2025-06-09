#pragma once

#include "duckdb/wrapped/std/memory.hpp"
#include "duckdb/wrapped/std/locale.hpp"
#include "duckdb/wrapped/std/sstream.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/shared_ptr.hpp"

namespace std {
template <class C> bool isspace(C c) {
	static_assert(sizeof(C) == 0, "Naked issspace is discontinued, either use StringUtil::CharacterIsSpace or duckdb_wrapped::std::isspace");
	return false;
}

#ifndef DUCKDB_ENABLE_DEPRECATED_API
template <class T, class... ARGS>
static duckdb::unique_ptr<T> make_unique(ARGS&&... __args) { // NOLINT: mimic std style
	static_assert(sizeof(T) == 0, "Use make_uniq instead of make_unique!");
	return nullptr;
}

template <class T, class... ARGS>
static duckdb::shared_ptr<T> make_shared(ARGS&&... __args) { // NOLINT: mimic std style
	static_assert(sizeof(T) == 0, "Use make_shared_ptr instead of make_shared!");
	return nullptr;
}
#endif // DUCKDB_ENABLE_DEPRECATED_API

} // namespace std

using std::isspace;
using std::make_shared;
using std::make_unique;
