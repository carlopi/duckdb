//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/operator/comparison_operators.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/types/string_type.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/hugeint.hpp"
#include "duckdb/common/types/interval.hpp"
#include "duckdb/common/helper.hpp"

#include <cstring>

namespace duckdb {

//===--------------------------------------------------------------------===//
// Comparison Operations
//===--------------------------------------------------------------------===//
struct Equals {
	template <class T>
	DUCKDB_API static inline bool Operation(const T& left, const T& right) {
		return left == right;
	}
};
struct NotEquals {
	template <class T>
	DUCKDB_API static inline bool Operation(const T& left, const T& right) {
		return !Equals::Operation(left, right);
	}
};

struct GreaterThan {
	template <class T>
	DUCKDB_API static inline bool Operation(const T& left, const T& right) {
		return left > right;
	}
};

struct GreaterThanEquals {
	template <class T>
	DUCKDB_API static inline bool Operation(const T& left, const T& right) {
		return left >= right;
	}
};

struct LessThan {
	template <class T>
	DUCKDB_API static inline bool Operation(const T& left, const T& right) {
		return GreaterThan::Operation(right, left);
	}
};

struct LessThanEquals {
	template <class T>
	DUCKDB_API static inline bool Operation(const T& left, const T& right) {
		return GreaterThanEquals::Operation(right, left);
	}
};

template <>
DUCKDB_API bool Equals::Operation(const float& left, const float& right);
template <>
DUCKDB_API bool Equals::Operation(const double& left, const double& right);

template <>
DUCKDB_API bool GreaterThan::Operation(const float& left, const float& right);
template <>
DUCKDB_API bool GreaterThan::Operation(const double& left, const double& right);

template <>
DUCKDB_API bool GreaterThanEquals::Operation(const float& left, const float& right);
template <>
DUCKDB_API bool GreaterThanEquals::Operation(const double& left, const double& right);

// Distinct semantics are from Postgres record sorting. NULL = NULL and not-NULL < NULL
// Deferring to the non-distinct operations removes the need for further specialisation.
// TODO: To reverse the semantics, swap left_null and right_null for comparisons
struct DistinctFrom {
	template <class T>
	static inline bool Operation(T left, T right, bool left_null, bool right_null) {
		return (left_null != right_null) || (!left_null && !right_null && NotEquals::Operation(left, right));
	}
};

struct NotDistinctFrom {
	template <class T>
	static inline bool Operation(T left, T right, bool left_null, bool right_null) {
		return (left_null && right_null) || (!left_null && !right_null && Equals::Operation(left, right));
	}
};

struct DistinctGreaterThan {
	template <class T>
	static inline bool Operation(T left, T right, bool left_null, bool right_null) {
		return GreaterThan::Operation(left_null, right_null) ||
		       (!left_null && !right_null && GreaterThan::Operation(left, right));
	}
};

struct DistinctGreaterThanNullsFirst {
	template <class T>
	static inline bool Operation(T left, T right, bool left_null, bool right_null) {
		return GreaterThan::Operation(right_null, left_null) ||
		       (!left_null && !right_null && GreaterThan::Operation(left, right));
	}
};

struct DistinctGreaterThanEquals {
	template <class T>
	static inline bool Operation(T left, T right, bool left_null, bool right_null) {
		return left_null || (!left_null && !right_null && GreaterThanEquals::Operation(left, right));
	}
};

struct DistinctLessThan {
	template <class T>
	static inline bool Operation(T left, T right, bool left_null, bool right_null) {
		return LessThan::Operation(left_null, right_null) ||
		       (!left_null && !right_null && LessThan::Operation(left, right));
	}
};

struct DistinctLessThanNullsFirst {
	template <class T>
	static inline bool Operation(T left, T right, bool left_null, bool right_null) {
		return LessThan::Operation(right_null, left_null) ||
		       (!left_null && !right_null && LessThan::Operation(left, right));
	}
};

struct DistinctLessThanEquals {
	template <class T>
	static inline bool Operation(T left, T right, bool left_null, bool right_null) {
		return right_null || (!left_null && !right_null && LessThanEquals::Operation(left, right));
	}
};

//===--------------------------------------------------------------------===//
// Specialized Boolean Comparison Operators
//===--------------------------------------------------------------------===//
template <>
inline bool GreaterThan::Operation(const bool& left, const bool& right) {
	return !right && left;
}
template <>
inline bool LessThan::Operation(const bool& left, const bool& right) {
	return !left && right;
}
//===--------------------------------------------------------------------===//
// Specialized String Comparison Operations
//===--------------------------------------------------------------------===//
struct StringComparisonOperators {
	template <bool INVERSE>
	static inline bool EqualsOrNot(const string_t& a, const string_t& b) {
		const uint64_t valA = *reinterpret_cast<const uint64_t*>(&a);
		const uint64_t valB = *reinterpret_cast<const uint64_t*>(&b);
		if (valA != valB) {
			return INVERSE;	// they can't represent the same underlying string
		}
		const uint64_t valA_2 = *(reinterpret_cast<const uint64_t*>(&a) + 1);
		const uint64_t valB_2 = *(reinterpret_cast<const uint64_t*>(&b) + 1);
		// they have the same length and same prefix!
		if (valA_2 == valB_2) {
			// either they are both inlined (so compare equal) or point to the same string (so compare equal)
			return !INVERSE;
		}
		if (!a.IsInlined()) {
			// 'long' strings -> compare pointed values
auto faster_memcmp_ = [](const char* left, const char* right, uint32_t common_length)->bool {
	const uint64_t* a = reinterpret_cast<const uint64_t*> (left);
	const uint64_t* b = reinterpret_cast<const uint64_t*> (right);
	uint32_t i = 0;
	do {
		if (*a != *b)
			return false;
		i += 8;
		a++;
		b++;
	} while (i + 8 < common_length);

	if (common_length >= 8) {
		a = reinterpret_cast<const uint64_t*> (left + common_length - 8);
		b = reinterpret_cast<const uint64_t*> (right + common_length - 8);
		if (*a != *b)
			return false;
	}
	return true;
};
		if (faster_memcmp_(a.value.pointer.ptr, b.value.pointer.ptr, a.GetSize())) {
//			if (memcmp(a.value.pointer.ptr, b.value.pointer.ptr, a.GetSize()) == 0) {
				return INVERSE ? false : true;
			}
		}
		// either they are short string of same lenght but different content
		//     or they point to string with different content
		//     either way, they can't represent the same underlying string
		return INVERSE;
	}
};

template <>
inline bool Equals::Operation(const string_t& left, const string_t& right) {
	return StringComparisonOperators::EqualsOrNot<false>(left, right);
}
template <>
inline bool NotEquals::Operation(const string_t& left, const string_t& right) {
	return StringComparisonOperators::EqualsOrNot<true>(left, right);
}

template <>
inline bool NotDistinctFrom::Operation(const string_t& left, const string_t& right, bool left_null, bool right_null) {
	return (left_null && right_null) ||
	       (!left_null && !right_null && StringComparisonOperators::EqualsOrNot<false>(left, right));
}
template <>
inline bool DistinctFrom::Operation(const string_t& left, const string_t& right, bool left_null, bool right_null) {
	return (left_null != right_null) ||
	       (!left_null && !right_null && StringComparisonOperators::EqualsOrNot<true>(left, right));
}

// compare up to shared length. if still the same, compare lengths
template <class OP>
static bool templated_string_compare_op(const string_t& left, const string_t& right) {
auto faster_memcmp_ = [](const char* left, const char* right, uint32_t common_length)->int {
	const uint64_t* a = reinterpret_cast<const uint64_t*> (left);
	const uint64_t* b = reinterpret_cast<const uint64_t*> (right);
	uint32_t i = 0;
	while (i + 8 < common_length) {
		if (*a != *b)
			return ((*a < *b) ) ? -1 : 1;
		i += 8;
		a++;
		b++;
	}
	uint32_t validSize = (common_length >= 8) ? 8 : common_length;
	uint64_t A = *reinterpret_cast<const uint64_t*> (left + common_length - validSize);
	uint64_t B = *reinterpret_cast<const uint64_t*> (right + common_length - validSize);

	A <<= ((8 - validSize) * 8);
	B <<= ((8 - validSize) * 8);
	if (A != B)
		return ((A < B)) ? -1 : 1;
	return 0;
};

	auto memcmp_res =
	  memcmp(left.GetDataUnsafe(), right.GetDataUnsafe(), MinValue<idx_t>(left.GetSize(), right.GetSize()));
	auto final_res = memcmp_res == 0 ? OP::Operation(left.GetSize(), right.GetSize()) : OP::Operation(memcmp_res, 0);
	return final_res;
}

template <>
inline bool GreaterThan::Operation(const string_t& left, const string_t& right) {
	return templated_string_compare_op<GreaterThan>(left, right);
}

template <>
inline bool GreaterThanEquals::Operation(const string_t& left, const string_t& right) {
	return templated_string_compare_op<GreaterThanEquals>(left, right);
}

template <>
inline bool LessThan::Operation(const string_t& left, const string_t& right) {
	return templated_string_compare_op<LessThan>(left, right);
}

template <>
inline bool LessThanEquals::Operation(const string_t& left, const string_t& right) {
	return templated_string_compare_op<LessThanEquals>(left, right);
}
//===--------------------------------------------------------------------===//
// Specialized Interval Comparison Operators
//===--------------------------------------------------------------------===//
template <>
inline bool Equals::Operation(const interval_t& left, const interval_t& right) {
	return Interval::Equals(left, right);
}
template <>
inline bool NotEquals::Operation(const interval_t& left, const interval_t& right) {
	return !Equals::Operation(left, right);
}
template <>
inline bool GreaterThan::Operation(const interval_t& left, const interval_t& right) {
	return Interval::GreaterThan(left, right);
}
template <>
inline bool GreaterThanEquals::Operation(const interval_t& left, const interval_t& right) {
	return Interval::GreaterThanEquals(left, right);
}
template <>
inline bool LessThan::Operation(const interval_t& left, const interval_t& right) {
	return GreaterThan::Operation(right, left);
}
template <>
inline bool LessThanEquals::Operation(const interval_t& left, const interval_t& right) {
	return GreaterThanEquals::Operation(right, left);
}

template <>
inline bool NotDistinctFrom::Operation(interval_t left, interval_t right, bool left_null, bool right_null) {
	return (left_null && right_null) || (!left_null && !right_null && Interval::Equals(left, right));
}
template <>
inline bool DistinctFrom::Operation(interval_t left, interval_t right, bool left_null, bool right_null) {
	return (left_null != right_null) || (!left_null && !right_null && !Equals::Operation(left, right));
}
inline bool operator<(const interval_t &lhs, const interval_t &rhs) {
	return LessThan::Operation(lhs, rhs);
}

//===--------------------------------------------------------------------===//
// Specialized Hugeint Comparison Operators
//===--------------------------------------------------------------------===//
template <>
inline bool Equals::Operation(const hugeint_t& left, const hugeint_t& right) {
	return Hugeint::Equals(left, right);
}
template <>
inline bool NotEquals::Operation(const hugeint_t& left, const hugeint_t& right) {
	return Hugeint::NotEquals(left, right);
}
template <>
inline bool GreaterThan::Operation(const hugeint_t& left, const hugeint_t& right) {
	return Hugeint::GreaterThan(left, right);
}
template <>
inline bool GreaterThanEquals::Operation(const hugeint_t& left, const hugeint_t& right) {
	return Hugeint::GreaterThanEquals(left, right);
}
template <>
inline bool LessThan::Operation(const hugeint_t& left, const hugeint_t& right) {
	return Hugeint::LessThan(left, right);
}
template <>
inline bool LessThanEquals::Operation(const hugeint_t& left, const hugeint_t& right) {
	return Hugeint::LessThanEquals(left, right);
}
} // namespace duckdb
