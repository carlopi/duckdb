//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/unordered_set.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/flat_hash_map.hpp"
#include <unordered_set>

namespace duckdb {

template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key> , class Allocator = std::allocator<const Key> >
using unordered_set = ska::flat_hash_set<Key, Hash, KeyEqual, Allocator>;

}
