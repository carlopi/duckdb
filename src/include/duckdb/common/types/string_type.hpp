//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/types/string_type.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/assert.hpp"
#include "duckdb/common/constants.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/numeric_utils.hpp"
#include "duckdb/common/limits.hpp"

#include <cstring>
#include <algorithm>

namespace duckdb {
//#define DUCKDB_DEBUG_NO_INLINE 1

struct string_t {
	friend struct StringComparisonOperators;

public:
	static constexpr idx_t PREFIX_BYTES = 4 * sizeof(char);
	static constexpr idx_t INLINE_BYTES = 15 * sizeof(char);
	static constexpr idx_t HEADER_SIZE = sizeof(uint32_t) + PREFIX_BYTES;
	static constexpr idx_t MAX_STRING_SIZE = NumericLimits<uint32_t>::Maximum();
#ifndef DUCKDB_DEBUG_NO_INLINE
	static constexpr idx_t PREFIX_LENGTH = PREFIX_BYTES;
	static constexpr idx_t INLINE_LENGTH = INLINE_BYTES;
#else
	static constexpr idx_t PREFIX_LENGTH = 0;
	static constexpr idx_t INLINE_LENGTH = 0;
#endif

	string_t() = default;
	explicit string_t(uint32_t len) {
		SetSize(len);
	}
	string_t(const char *data, uint32_t len) {
		SetSize(len);
		D_ASSERT(data || GetSize() == 0);
		if (IsInlined()) {
			// zero initialize the prefix first
			// this makes sure that strings with length smaller than 4 still have an equal prefix
			memset(value.inlined.inlined + 1, 0, INLINE_BYTES);
			if (GetSize() == 0) {
				return;
			}
			// small string: inlined
			memcpy(value.inlined.inlined + 1, data, GetSize());
		} else {
			// large string: store pointer
#ifndef DUCKDB_DEBUG_NO_INLINE
			memcpy(value.inlined.inlined + 1, data, PREFIX_LENGTH);
#else
			memset(value.inlined.inlined + 1, 0, PREFIX_BYTES);
#endif
			value.pointer.ptr = (char *)data; // NOLINT
		}
	}

	string_t(const char *data) // NOLINT: Allow implicit conversion from `const char*`
	    : string_t(data, UnsafeNumericCast<uint32_t>(strlen(data))) {
	}
	string_t(const string &value) // NOLINT: Allow implicit conversion from `const char*`
	    : string_t(value.c_str(), UnsafeNumericCast<uint32_t>(value.size())) {
	}

	bool IsInlined() const {
		if (value.pointer.lengthz == 0)
			return true;
		uint8_t k16 = value.x.inlined[0] - (uint8_t)1;
		if (k16 >= 240u)
			return true;
		return false;	
	}

	const char *GetData() const {
		return IsInlined() ? (const char *)(value.inlined.inlined + 1) : value.pointer.ptr;
	}
	const char *GetDataUnsafe() const {
		return GetData();
	}

	char *GetDataWriteable() const {
		return const_cast<char *>(IsInlined() ? (value.inlined.inlined + 1) : value.pointer.ptr);
	}

	char *GetPrefixWriteable() {
		return value.inlined.inlined + 1;
	}
	const char *GetPrefix() const {
		return (value.inlined.inlined + 1);
	}

	idx_t GetSize() const {
		if (value.pointer.lengthz == 0)
			return 0;
		uint8_t k16 = value.x.inlined[0];
		k16--;
		if (k16 >= 240u)
			return (k16 - (uint8_t)240 + (uint8_t)1);		
	//	uint64_t y = value.pointer.lengthz;
		//uint32_t x = ((y << 40u) >> 40u) | ((y >> 56u) << 24u);
		return (k16 << 24u) | (value.x.inlined[7] << 16u) | (value.x.inlined[6] << 8u) | value.x.inlined[5];
		//uint32_t x = (k16 << 24) | ((value.pointer.lengthz >> 32) & ( 0xffffff00)) >> 8);
		//return uint32_t(x );// + 16);
	}

	bool Empty() const {
		return value.pointer.lengthz == 0;
	}

	void SetSize(uint32_t len) {
		value.pointer.lengthz = 0;
	D_ASSERT(GetSize() == 0);
		if (len == 0)	{
	return;
}	
	D_ASSERT(GetSize() == 0);
		if (len < 16u)
{
			value.x.inlined[0] = (len + 240u);
	D_ASSERT(GetSize() == len);
	return;
}
	D_ASSERT(GetSize() == 0);
		{
			//value.pointer.lengthz = (len & 0x00ffffff) << 8;
			value.x.inlined[0] = len >> 24;
			value.x.inlined[0]++;
			value.x.inlined[7] = (len & 0x00ff0000) >> 16;
			value.x.inlined[6] = (len & 0x0000ff00) >> 8;
			value.x.inlined[5] = len & 0x000000ff;
			//len -= 16;
			//value.pointer.lengthz = (((uint64_t)len) << (uint64_t)32u) | len;
//D_ASSERT(!IsInlined());
		}
	D_ASSERT(GetSize() == len);
	//	value.inlined.inlined[0] = 0;
	//	value.inlined.inlined[1] = 0;
	//	value.inlined.inlined[2] = 0;
	//	value.inlined.inlined[3] = 0;
	D_ASSERT(GetSize() == len);
	} 
	string GetString() const {
		return string(GetData(), GetSize());
	}

	explicit operator string() const {
		return GetString();
	}

	char *GetPointer() const {
		D_ASSERT(!IsInlined());
		return value.pointer.ptr;
	}

	void SetPointer(char *new_ptr) {
		D_ASSERT(!IsInlined());
		value.pointer.ptr = new_ptr;
	}

	void Finalize() {
		// set trailing NULL byte
		if (GetSize() <= INLINE_LENGTH) {
			// fill prefix with zeros if the length is smaller than the prefix length
			for (idx_t i = GetSize(); i < INLINE_BYTES; i++) {
				value.inlined.inlined[i+1] = '\0';
			}
		} else {
			// copy the data into the prefix
#ifndef DUCKDB_DEBUG_NO_INLINE
			auto dataptr = (char *)GetData();
			memcpy(value.inlined.inlined + 1, dataptr, PREFIX_LENGTH);
#else
			memset(value.inlined.inlined + 1, 0, PREFIX_BYTES);
#endif
		}
	}

	void Verify() const;
	void VerifyUTF8() const;
	void VerifyCharacters() const;
	void VerifyNull() const;

	struct StringComparisonOperators {
		static inline bool Equals(const string_t &a, const string_t &b) {
#ifdef DUCKDB_DEBUG_NO_INLINE
			if (a.GetSize() != b.GetSize()) {
				return false;
			}
			return (memcmp(a.GetData(), b.GetData(), a.GetSize()) == 0);
#endif
			uint64_t a_bulk_comp = Load<uint64_t>(const_data_ptr_cast(&a));
			uint64_t b_bulk_comp = Load<uint64_t>(const_data_ptr_cast(&b));
			if (a_bulk_comp != b_bulk_comp) {
				// Either length or prefix are different -> not equal
				return false;
			}
			// they have the same length and same prefix!
			a_bulk_comp = Load<uint64_t>(const_data_ptr_cast(&a) + 8u);
			b_bulk_comp = Load<uint64_t>(const_data_ptr_cast(&b) + 8u);
			if (a_bulk_comp == b_bulk_comp) {
				// either they are both inlined (so compare equal) or point to the same string (so compare equal)
				return true;
			}
			if (!a.IsInlined()) {
				// 'long' strings of the same length -> compare pointed value
				if (memcmp(a.value.pointer.ptr, b.value.pointer.ptr, a.GetSize()) == 0) {
					return true;
				}
			}
			// either they are short string of same length but different content
			//     or they point to string with different content
			//     either way, they can't represent the same underlying string
			return false;
		}
		// compare up to shared length. if still the same, compare lengths
		static bool GreaterThan(const string_t &left, const string_t &right) {
			const uint32_t left_length = UnsafeNumericCast<uint32_t>(left.GetSize());
			const uint32_t right_length = UnsafeNumericCast<uint32_t>(right.GetSize());
			const uint32_t min_length = std::min<uint32_t>(left_length, right_length);

#ifndef DUCKDB_DEBUG_NO_INLINE
			uint32_t a_prefix = Load<uint32_t>(const_data_ptr_cast(left.GetPrefix()));
			uint32_t b_prefix = Load<uint32_t>(const_data_ptr_cast(right.GetPrefix()));

			// Utility to move 0xa1b2c3d4 into 0xd4c3b2a1, basically inverting the order byte-a-byte
			auto byte_swap = [](uint32_t v) -> uint32_t {
				uint32_t t1 = (v >> 16u) | (v << 16u);
				uint32_t t2 = t1 & 0x00ff00ff;
				uint32_t t3 = t1 & 0xff00ff00;
				return (t2 << 8u) | (t3 >> 8u);
			};

			// Check on prefix -----
			// We dont' need to mask since:
			//	if the prefix is greater(after bswap), it will stay greater regardless of the extra bytes
			// 	if the prefix is smaller(after bswap), it will stay smaller regardless of the extra bytes
			//	if the prefix is equal, the extra bytes are guaranteed to be /0 for the shorter one

			if (a_prefix != b_prefix) {
				return byte_swap(a_prefix) > byte_swap(b_prefix);
			}
#endif
			auto memcmp_res = memcmp(left.GetData(), right.GetData(), min_length);
			return memcmp_res > 0 || (memcmp_res == 0 && left_length > right_length);
		}
	};

	bool operator==(const string_t &r) const {
		return StringComparisonOperators::Equals(*this, r);
	}

	bool operator!=(const string_t &r) const {
		return !(*this == r);
	}

	bool operator>(const string_t &r) const {
		return StringComparisonOperators::GreaterThan(*this, r);
	}
	bool operator<(const string_t &r) const {
		return r > *this;
	}

private:
	union {
		struct {
			uint64_t lengthz;
			char *ptr;
		} pointer;
		struct {
			char inlined[16];
		} inlined;
		struct {
			uint8_t inlined[16];
		} x;
	} value;
};

} // namespace duckdb
