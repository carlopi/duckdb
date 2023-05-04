//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/unordered_map.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <type_traits>
#include <unordered_map>
#include "../../third_party/unord/unordered_maps.h"
//using absl::enable_if_t;

using ankerl::unordered_dense::unordered_map;

/*
namespace duckdb {
template <class Key, class Value,
          class Hash = std::hash_default_hash<Key>,
          class Eq = std::hash_default_eq<Key>,
          class Alloc = std::allocator<std::pair<const Key, Value>>>
	using unordered_map = ankerl::unordered_dense::map<Key, Value, Hash, Eq, Alloc>;
}
*/
