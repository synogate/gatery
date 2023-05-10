/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "Range.h"
#include "Exceptions.h"

#include <boost/rational.hpp>


#include <cstdint>
#include <bit>


#include <immintrin.h>

#if defined(_M_AMD64) || defined(__amd64__)
#define AMD64 1
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace gtry::utils 
{
	using ::std::popcount;

template<typename T>
bool isPow2(T v) { return popcount(v) == 1; }

template<typename T>
T Log2(T v)
{
	HCL_ASSERT(v > 0);
	T ret = 0;
	while (v >>= 1)
		++ret;
	return ret;
}

template<typename T>
T Log2C(T v)
{
	HCL_ASSERT(v > 0);
	if (v == 1)
		return 0;

	return Log2(v - 1) + 1;
}

#ifdef _MSC_VER
template<> inline uint16_t Log2(uint16_t v) { HCL_ASSERT(v > 0); return 15 - __lzcnt16(v); }
template<> inline uint32_t Log2(uint32_t v) { HCL_ASSERT(v > 0); return 31 - __lzcnt(v); }

# ifdef AMD64
template<> inline uint64_t Log2(uint64_t v) { HCL_ASSERT(v > 0); return 63 - __lzcnt64(v); }
# endif
#else
template<> inline uint32_t Log2(uint32_t v) { HCL_ASSERT(v > 0); return 31 - __builtin_clz(v); }

# ifdef AMD64
template<> inline uint64_t Log2(uint64_t v) { HCL_ASSERT(v > 0); return 63 - __builtin_clzll(v); }
# endif
#endif


inline size_t Log2(boost::rational<std::uint64_t> v)
{
	return Log2(v.numerator() / v.denominator());
}

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T nextPow2(T v)
{
	v--;
	for (T bit = 1; bit < sizeof(T) * 8; bit <<= 1)
		v |= v >> bit;
	v++;
	return v;
}

template<typename T>
inline T andNot(T a, T b) {
	return ~a & b;
}

#ifdef __BMI__
#ifdef AMD64
template<>
inline std::uint64_t andNot(std::uint64_t a, std::uint64_t b) {
#ifdef _MSC_VER
	return _andn_u64(a, b);
#else
	return __andn_u64(a, b);
#endif
}
#endif
template<>
inline std::uint32_t andNot(std::uint32_t a, std::uint32_t b) {
#ifdef _MSC_VER
	return _andn_u32(a, b);
#else
	return __andn_u32(a, b);
#endif
}
#endif



	
inline bool bitExtract(std::uint64_t a, unsigned idx) {
#if 1
	return a & (1ull << idx);
#else
	return _bittest64(&a, idx);
#endif
}

inline bool bitExtract(const void *a, size_t idx) {
#if 1
	return ((std::uint64_t*)a)[idx/64] & (1ull << (idx % 64));
#else
	return _bittest64((std::uint64_t)&a, idx);
#endif
}
   

	
inline void bitSet(std::uint64_t &a, unsigned idx) {
#if 1
	a |= 1ull << idx;
#else
	_bittestandset64(&a, idx);
#endif
}

inline void bitSet(const void *a, size_t idx) {
#if 1
	((std::uint64_t*)a)[idx/64] |= 1ull << (idx % 64);
#else
	_bittestandset64((std::uint64_t)&a, idx);
#endif
}


inline void bitClear(std::uint64_t &a, unsigned idx) {
#if 1
	a = andNot<std::uint64_t>(1ull << idx, a);
#else
	_bittestandreset64(&a, idx);
#endif
}

inline void bitClear(const void *a, size_t idx) {
#if 1
	std::uint64_t &v = ((std::uint64_t*)a)[idx/64];
	v = andNot<std::uint64_t>(1ull << (idx % 64), v);
#else
	_bittestandreset64((std::uint64_t)&a, idx);
#endif	
}



inline void bitToggle(std::uint64_t &a, unsigned idx) {
#if 1
	a = a ^ (1ull << idx);
#else
	_bittestandcomplement64(&a, idx);
#endif
}

inline void bitToggle(const void *a, size_t idx) {
#if 1
	std::uint64_t &v = ((std::uint64_t*)a)[idx/64];
	v = v ^ (1ull << (idx % 64));
#else
	_bittestandcomplement64((std::uint64_t)&a, idx);
#endif
}

template<typename T = std::size_t>
inline T bitMaskRange(size_t start, size_t count) {
	if (count >= sizeof(T) * 8)
		return ~T(0) << start;
	return ((T{ 1 } << count) - 1) << start;
}

template<typename T = std::size_t>
inline bool isMaskSet(T a, size_t start, size_t count) {
	T mask = bitMaskRange<T>(start, count);
	return (a & mask) == mask;
}

template<typename T>
inline T bitfieldExtract(T a, size_t start, size_t count) {
	start &= 0xFF;
	count &= 0xFF;
	return (a >> start) & bitMaskRange(0, count);
}

#ifdef __BMI__
#ifdef AMD64
template<>
inline std::uint64_t bitfieldExtract(std::uint64_t a, size_t start, size_t count) {
	return _bextr_u64(a, (unsigned int)start, (unsigned int)count);
}
#endif

template<>
inline std::uint32_t bitfieldExtract(std::uint32_t a, size_t start, size_t count) {
	return _bextr_u32(a, (unsigned int)start, (unsigned int)count);
}
#endif

template<typename T>
inline T bitfieldInsert(T a, size_t start, size_t count, T v) {
	auto mask = bitMaskRange<T>(start, count);
	return andNot(mask, a) | (mask & (v << start));
}



#ifdef __BMI2__
inline std::uint64_t parallelBitExtract(std::uint64_t a, std::uint64_t mask) {
	return _pext_u64(a, mask);
}
#else
inline std::uint64_t parallelBitExtract(std::uint64_t a, std::uint64_t mask) {
	std::uint64_t result = 0;
	unsigned j = 0;
	for (auto i : utils::Range(64))
		if (bitExtract(mask, i)) {
			if (bitExtract(a, i))
				bitSet(result, j);
			j++;
		}
	return result;
}
#endif

#ifdef __BMI2__
inline std::uint64_t parallelBitDeposit(std::uint64_t a, std::uint64_t mask) {
	return _pdep_u64(a, mask);
}
#else
inline std::uint64_t parallelBitDeposit(std::uint64_t a, std::uint64_t mask) {
	std::uint64_t result = 0;
	unsigned j = 0;
	for (auto i : utils::Range(64))
		if (bitExtract(mask, i)) {
			if (bitExtract(a, j))
				bitSet(result, i);
			j++;
		}
	return result;
}
#endif

template<typename T>
inline T lowestSetBitMask(T val)
{
	return val & (0-val);
}

inline std::uint8_t flipEndian(std::uint8_t v) { return v; }
inline std::int8_t flipEndian(std::int8_t v) { return v; }

inline std::uint16_t flipEndian(std::uint16_t v) { return (v << 8) | (v >> 8); }
inline std::int16_t flipEndian(std::int16_t v) { return (std::int16_t) flipEndian((std::uint16_t)v); }

inline std::uint32_t flipEndian(std::uint32_t v) { return (std::uint32_t)flipEndian((std::uint16_t)v) << 16 | flipEndian((std::uint16_t)(v >> 16)); }
inline std::int32_t flipEndian(std::int32_t v) { return (std::int32_t) flipEndian((std::uint32_t)v); }

inline std::uint64_t flipEndian(std::uint64_t v) { return (std::uint64_t)flipEndian((std::uint32_t)v) << 32 | flipEndian((std::uint32_t)(v >> 32)); }
inline std::int64_t flipEndian(std::int64_t v) { return (std::int64_t) flipEndian((std::uint64_t)v); }

}
	
