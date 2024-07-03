//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/query_result.hpp"
#include "duckdb/main/appender.hpp"
#include <random>
#include <iostream>

namespace duckdb {
  class random_device {
	std::random_device rnd;
  public:
    // types
    using result_type = unsigned int;
 
    // generator characteristics
    static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
 
    // constructors
    random_device() : rnd() {}
    explicit random_device(const string& token) : rnd(token) {}
 
    // generating functions
    result_type operator()() {
		auto x = rnd();
		if (x % 1024 == 127) {
			throw std::runtime_error("BOOM?");
		}
		return x;
	}
 
    // property functions
    double entropy() const noexcept {
		return rnd.entropy();
}
 
    // no copy functions
    random_device(const random_device&) = delete;
    void operator=(const random_device&) = delete;
  };
}
