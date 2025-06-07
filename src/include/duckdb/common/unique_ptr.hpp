#pragma once

#include "duckdb/common/exception.hpp"
#include "duckdb/common/likely.hpp"
#include "duckdb/common/memory_safety.hpp"

#include <memory>
#include <type_traits>

namespace duckdb {

template <class DATA_TYPE, bool SAFE, class DELETER>
class unique_ptr_impl : public std::unique_ptr<DATA_TYPE, DELETER> { // NOLINT: naming
public:
	using original = std::unique_ptr<DATA_TYPE, DELETER>;
	using original::original; // NOLINT
	using pointer = typename original::pointer;

private:
	static inline void AssertNotNull(const bool null) {
#if defined(DUCKDB_DEBUG_NO_SAFETY) || defined(DUCKDB_CLANG_TIDY)
		return;
#else
		if (DUCKDB_UNLIKELY(null)) {
			throw duckdb::InternalException("Attempted to dereference unique_ptr that is NULL!");
		}
#endif
	}

public:
	typename std::add_lvalue_reference<DATA_TYPE>::type operator*() const { // NOLINT: hiding on purpose
		const auto ptr = original::get();
		if (MemorySafety<SAFE>::ENABLED) {
			AssertNotNull(!ptr);
		}
		return *ptr;
	}

	typename original::pointer operator->() const { // NOLINT: hiding on purpose
		const auto ptr = original::get();
		if (MemorySafety<SAFE>::ENABLED) {
			AssertNotNull(!ptr);
		}
		return ptr;
	}

#ifdef DUCKDB_CLANG_TIDY
	// This is necessary to tell clang-tidy that it reinitializes the variable after a move
	[[clang::reinitializes]]
#endif
	inline void
	reset(typename original::pointer ptr = typename original::pointer()) noexcept { // NOLINT: hiding on purpose
		original::reset(ptr);
	}
};

template <class DATA_TYPE, bool SAFE, class DELETER>
class unique_ptr_impl<DATA_TYPE[], SAFE, DELETER> : public std::unique_ptr<DATA_TYPE[], DELETER> {
public:
	using original = std::unique_ptr<DATA_TYPE[], DELETER>;
	using original::original;

private:
	static inline void AssertNotNull(const bool null) {
#if defined(DUCKDB_DEBUG_NO_SAFETY) || defined(DUCKDB_CLANG_TIDY)
		return;
#else
		if (DUCKDB_UNLIKELY(null)) {
			throw duckdb::InternalException("Attempted to dereference unique_ptr that is NULL!");
		}
#endif
	}

public:
	typename std::add_lvalue_reference<DATA_TYPE>::type operator[](size_t __i) const { // NOLINT: hiding on purpose
		const auto ptr = original::get();
		if (MemorySafety<SAFE>::ENABLED) {
			AssertNotNull(!ptr);
		}
		return ptr[__i];
	}
};

template <typename T>
using unique_ptr = unique_ptr_impl<T, true, std::default_delete<T>>;

template <typename T>
using unique_array = unique_ptr_impl<T[], true, std::default_delete<T[]>>;

template <typename T>
using unsafe_unique_array = unique_ptr_impl<T[], false, std::default_delete<T[]>>;

template <typename T>
using unsafe_unique_ptr = unique_ptr_impl<T, false, std::default_delete<T>>;

} // namespace duckdb
