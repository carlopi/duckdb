//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/unordered_map.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/flat_hash_map.hpp"
#include <unordered_map>

namespace duckdb {

template <class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key> , class Allocator = std::allocator<std::pair<const Key, T> > >
using unordered_map = ska::flat_hash_map<Key, T, Hash, KeyEqual, Allocator>;

}
