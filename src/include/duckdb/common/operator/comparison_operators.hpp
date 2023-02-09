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
	DUCKDB_API static inline bool Operation(T left, T right) {
		return left == right;
	}
};
struct NotEquals {
	template <class T>
	DUCKDB_API static inline bool Operation(T left, T right) {
		return !Equals::Operation(left, right);
	}
};

struct GreaterThan {
	template <class T>
	DUCKDB_API static inline bool Operation(T left, T right) {
		return left > right;
	}
};

struct GreaterThanEquals {
	template <class T>
	DUCKDB_API static inline bool Operation(T left, T right) {
		return left >= right;
	}
};

struct LessThan {
	template <class T>
	DUCKDB_API static inline bool Operation(T left, T right) {
		return GreaterThan::Operation(right, left);
	}
};

struct LessThanEquals {
	template <class T>
	DUCKDB_API static inline bool Operation(T left, T right) {
		return GreaterThanEquals::Operation(right, left);
	}
};

template <>
DUCKDB_API bool Equals::Operation(float left, float right);
template <>
DUCKDB_API bool Equals::Operation(double left, double right);

template <>
DUCKDB_API bool GreaterThan::Operation(float left, float right);
template <>
DUCKDB_API bool GreaterThan::Operation(double left, double right);

template <>
DUCKDB_API bool GreaterThanEquals::Operation(float left, float right);
template <>
DUCKDB_API bool GreaterThanEquals::Operation(double left, double right);

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
inline bool GreaterThan::Operation(bool left, bool right) {
	return !right && left;
}
template <>
inline bool LessThan::Operation(bool left, bool right) {
	return !left && right;
}
//===--------------------------------------------------------------------===//
// Specialized String Comparison Operations
//===--------------------------------------------------------------------===//
struct StringComparisonOperators {
	template <bool INVERSE>
	static inline bool EqualsOrNot(const string_t a, const string_t b) {
		if (memcmp(&a, &b, string_t::HEADER_SIZE) != 0) {
			return INVERSE;	// they can't represent the same underlying string
		}
		// they have the same length and same prefix!
		if (memcmp(&a, &b, sizeof(string_t)) == 0) {
			// either they are both inlined (so compare equal) or point to the same string (so compare equal)
			return !INVERSE;
		}
		if (!a.IsInlined()) {
			// 'long' strings -> compare pointed values
			if (memcmp(a.value.pointer.ptr, b.value.pointer.ptr, a.GetSize()) == 0) {
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
inline bool Equals::Operation(string_t left, string_t right) {
	return StringComparisonOperators::EqualsOrNot<false>(left, right);
}
template <>
inline bool NotEquals::Operation(string_t left, string_t right) {
	return StringComparisonOperators::EqualsOrNot<true>(left, right);
}

template <>
inline bool NotDistinctFrom::Operation(string_t left, string_t right, bool left_null, bool right_null) {
	return (left_null && right_null) ||
	       (!left_null && !right_null && StringComparisonOperators::EqualsOrNot<false>(left, right));
}
template <>
inline bool DistinctFrom::Operation(string_t left, string_t right, bool left_null, bool right_null) {
	return (left_null != right_null) ||
	       (!left_null && !right_null && StringComparisonOperators::EqualsOrNot<true>(left, right));
}

// compare up to shared length. if still the same, compare lengths
template <class OP>
static bool templated_string_compare_op(string_t left, string_t right) {
	auto memcmp_res =
	    memcmp(left.GetDataUnsafe(), right.GetDataUnsafe(), MinValue<idx_t>(left.GetSize(), right.GetSize()));
	auto final_res = memcmp_res == 0 ? OP::Operation(left.GetSize(), right.GetSize()) : OP::Operation(memcmp_res, 0);
	return final_res;
}

template <>
inline bool GreaterThan::Operation(string_t left, string_t right) {
	return templated_string_compare_op<GreaterThan>(left, right);
}

template <>
inline bool GreaterThanEquals::Operation(string_t left, string_t right) {
	return templated_string_compare_op<GreaterThanEquals>(left, right);
}

template <>
inline bool LessThan::Operation(string_t left, string_t right) {
	return templated_string_compare_op<LessThan>(left, right);
}

template <>
inline bool LessThanEquals::Operation(string_t left, string_t right) {
	return templated_string_compare_op<LessThanEquals>(left, right);
}
//===--------------------------------------------------------------------===//
// Specialized Interval Comparison Operators
//===--------------------------------------------------------------------===//
template <>
inline bool Equals::Operation(interval_t left, interval_t right) {
	return Interval::Equals(left, right);
}
template <>
inline bool NotEquals::Operation(interval_t left, interval_t right) {
	return !Equals::Operation(left, right);
}
template <>
inline bool GreaterThan::Operation(interval_t left, interval_t right) {
	return Interval::GreaterThan(left, right);
}
template <>
inline bool GreaterThanEquals::Operation(interval_t left, interval_t right) {
	return Interval::GreaterThanEquals(left, right);
}
template <>
inline bool LessThan::Operation(interval_t left, interval_t right) {
	return GreaterThan::Operation(right, left);
}
template <>
inline bool LessThanEquals::Operation(interval_t left, interval_t right) {
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
inline bool Equals::Operation(hugeint_t left, hugeint_t right) {
	return Hugeint::Equals(left, right);
}
template <>
inline bool NotEquals::Operation(hugeint_t left, hugeint_t right) {
	return Hugeint::NotEquals(left, right);
}
template <>
inline bool GreaterThan::Operation(hugeint_t left, hugeint_t right) {
	return Hugeint::GreaterThan(left, right);
}
template <>
inline bool GreaterThanEquals::Operation(hugeint_t left, hugeint_t right) {
	return Hugeint::GreaterThanEquals(left, right);
}
template <>
inline bool LessThan::Operation(hugeint_t left, hugeint_t right) {
	return Hugeint::LessThan(left, right);
}
template <>
inline bool LessThanEquals::Operation(hugeint_t left, hugeint_t right) {
	return Hugeint::LessThanEquals(left, right);
}
} // namespace duckdb
