#include "duckdb/common/types/hash.hpp"

#include "duckdb/common/helper.hpp"
#include "duckdb/common/types/string_type.hpp"
#include "duckdb/common/types/interval.hpp"

#include <functional>
#include <cmath>

namespace duckdb {

template <>
hash_t Hash(uint64_t val) {
	return murmurhash64(val);
}

template <>
hash_t Hash(int64_t val) {
	return murmurhash64((uint64_t)val);
}

template <>
hash_t Hash(hugeint_t val) {
	return murmurhash64(val.lower) ^ murmurhash64(val.upper);
}

template <class T>
struct FloatingPointEqualityTransform {
	static void OP(T &val) {
		if (val == (T)0.0) {
			// Turn negative zero into positive zero
			val = (T)0.0;
		} else if (std::isnan(val)) {
			val = std::numeric_limits<T>::quiet_NaN();
		}
	}
};

template <>
hash_t Hash(float val) {
	static_assert(sizeof(float) == sizeof(uint32_t), "");
	FloatingPointEqualityTransform<float>::OP(val);
	uint32_t uval = Load<uint32_t>((const_data_ptr_t)&val);
	return murmurhash64(uval);
}

template <>
hash_t Hash(double val) {
	static_assert(sizeof(double) == sizeof(uint64_t), "");
	FloatingPointEqualityTransform<double>::OP(val);
	uint64_t uval = Load<uint64_t>((const_data_ptr_t)&val);
	return murmurhash64(uval);
}

template <>
hash_t Hash(interval_t val) {
	return Hash(val.days) ^ Hash(val.months) ^ Hash(val.micros);
}

template <>
hash_t Hash(const char *str) {
	return Hash(str, strlen(str));
}

template <>
hash_t Hash(char *val) {
	return Hash<const char *>(val);
}

// MIT License
// Copyright (c) 2018-2021 Martin Ankerl
// https://github.com/martinus/robin-hood-hashing/blob/3.11.5/LICENSE
hash_t HashBytesInner(const void *ptr, size_t len) noexcept {
	static constexpr uint64_t M = UINT64_C(0xc6a4a7935bd1e995);
	static constexpr uint64_t SEED = UINT64_C(0xe17a1465);
	static constexpr unsigned int R = 47;

	auto const *const data64 = static_cast<uint64_t const *>(ptr);
	uint64_t h = SEED ^ (len * M);

	auto const * data8 = reinterpret_cast<uint8_t const *>(ptr);
	uint64_t p = (uint64_t)(data8);
uint64_t k = 0;
	if (p % 8)
{
	size_t shift = ((size_t)p%8);
	auto const * end = data8 + len;
	
	uint64_t s[2] = {0u, 0u};
/*	for (size_t i = shift; i<8u; i++)
	{
		(reinterpret_cast<uint8_t*>(s+1))[i-shift] = *data8;
		data8++;
	}
*/
	uint8_t* writePtr = reinterpret_cast<uint8_t*>(s+1);
	switch (shift) {
	case 1:
		*writePtr = *data8;
		writePtr++;
		data8++;
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 2:
		*writePtr = *data8;
		writePtr++;
		data8++;
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 3:
		*writePtr = *data8;
		writePtr++;
		data8++;
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 4:
		*writePtr = *data8;
		writePtr++;
		data8++;
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 5:
		*writePtr = *data8;
		writePtr++;
		data8++;
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 6:
		*writePtr = *data8;
		writePtr++;
		data8++;
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 7:
		*writePtr = *data8;
		writePtr++;
		data8++;
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 0:
		break;
	}

shift *= 8u;


	//........hello...

	
	s[0] = s[1] << (shift);

	//...hello xyzkjulm


	while (data8 + 8 <= end)
	{
		s[1] = *reinterpret_cast<uint64_t const*>(data8);
	
		s[0] >>= (shift);
		s[0] |= (s[1] << (64u - shift));

		k = s[0];
		//printf("div.main\t%p\n", k);
		k *= M;
		k ^= k >> R;
		k *= M;

		h ^= k;
		h *= M;

		data8+=8;
	
		s[0] = (s[1] << ( shift));
s[0] = s[1];
	}
	s[1] = 0u;

size_t xx = end - data8;


	union {
		uint64_t u64;
		uint8_t u8[8];
	} u;
	uint64_t &k1 = u.u64;
	k1 = 0;
	switch (xx) {
	case 7:
		u.u8[6] = data8[6];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 6:
		u.u8[5] = data8[5];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 5:
		u.u8[4] = data8[4];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 4:
		u.u8[3] = data8[3];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 3:
		u.u8[2] = data8[2];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 2:
		u.u8[1] = data8[1];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 1:
		u.u8[0] = data8[0];

		break;
	case 0:
		break;
	}

	s[1] = k1;



/*
	while (data8 < end)
	{
		(reinterpret_cast<uint8_t*>(s+1))[xx] = *data8;
		data8++;
		xx++;
	}
*/
	size_t remaining = xx + (64u - shift)/8u;

		s[0] >>= (shift);
		s[0] |= (s[1] << (64u- shift));
		s[1] >>= (shift);
	if (remaining >= 8u)
	{
		k = s[0];
	//printf("dis.rem8u\t%p\n", k);
		k *= M;
		k ^= k >> R;
		k *= M;

		h ^= k;
		h *= M;
		s[0] = s[1];
		s[0] = (s[1]);
	remaining -=8u;
	}	

{
	if (remaining > 0u)
	{

		k = s[0];
	//printf("dis.last\t%p\n", k);
		k *= M;
		k ^= k >> R;
		k *= M;

		h ^= k;
	}
	h *= M;
}

}
else
{
	size_t const n_blocks = len / 8;
	for (size_t i = 0; i < n_blocks; ++i) {
		auto k = Load<uint64_t>(reinterpret_cast<const_data_ptr_t>(data64 + i));

	//	printf("aligned.1\t%p\n", k);
		k *= M;
		k ^= k >> R;
		k *= M;

		h ^= k;
		h *= M;
	}
	data8 += n_blocks * 8u;
	len &= 7U;

	union {
		uint64_t u64;
		uint8_t u8[8];
	} u;
	uint64_t &k1 = u.u64;
	k1 = 0;
	switch (len) {
	case 7:
		u.u8[6] = data8[6];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 6:
		u.u8[5] = data8[5];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 5:
		u.u8[4] = data8[4];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 4:
		u.u8[3] = data8[3];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 3:
		u.u8[2] = data8[2];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 2:
		u.u8[1] = data8[1];
		DUCKDB_EXPLICIT_FALLTHROUGH;
	case 1:
		u.u8[0] = data8[0];

		//printf("aligned.2\t%p\n", k1);
		k1 *= M;
		k1 ^= k1 >> R;
		k1 *= M;

		h ^= k1;
		break;
	case 0:
		// Nothing to do;
		break;
	}

	h *= M;
}
	h ^= h >> R;
	h *= M;
	h ^= h >> R;
	return static_cast<hash_t>(h);
}


hash_t HashBytes(const void *ptr, size_t len) noexcept {
	return HashBytesInner(ptr, len);

	//printf("hashing...%d\n", len);
	void* x = malloc(len);
	memcpy(x, ptr, len);
	auto X = HashBytesInner(ptr, len);
	auto Y = HashBytesInner(x, len);
	if (X != Y)
{
	printf("\n%d\t%s\n", len, ptr);
	printf("%llu\t--\t%llu\n", X, Y);
}
	D_ASSERT(X == Y);
	free(x);
	return Y;

}

hash_t Hash(const char *val, size_t size) {
	return HashBytes((const void *)val, size);
}

hash_t Hash(uint8_t *val, size_t size) {
	return HashBytes((void *)val, size);
}

template <>
hash_t Hash(string_t val) {
#ifdef DUCKDB_DEBUG_NO_INLINE
	return HashBytes((const void *)val.GetDataUnsafe(), val.GetSize());
#endif
	if (val.GetSize() <= 8u) {
		static constexpr uint64_t M = UINT64_C(0xc6a4a7935bd1e995);
		static constexpr uint64_t SEED = UINT64_C(0xe17a1465);
		static constexpr unsigned int R = 47;

		const_data_ptr_t data64 = static_cast<const_data_ptr_t>(val.GetInlined());
		uint64_t h = SEED ^ (val.GetSize() * M);


		uint32_t t1 = *reinterpret_cast<const uint32_t *>(data64);
		uint32_t t2 = *(reinterpret_cast<const uint32_t *>(data64) + 1);

		uint64_t k = ((((uint64_t)t1) << 32ull) | t2);

		k *= M;
		k ^= k >> R;
		k *= M;

		h ^= k;
		h *= M;

/*		k = *(reinterpret_cast<const uint32_t *>(data64) + 2);
		k <<= 32u;
		k *= M;
		k ^= k >> R;
		k *= M;

		h ^= k;
		h *= M;
*/
		h ^= h >> R;
		h *= M;
		h ^= h >> R;
		return static_cast<hash_t>(h);
	} else {
		return HashBytes((const void *)val.GetDataUnsafe(), val.GetSize());
	}
}

} // namespace duckdb
