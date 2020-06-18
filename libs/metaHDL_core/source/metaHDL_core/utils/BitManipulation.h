#pragma once

#include "Range.h"

#include <cstdint>

#include <immintrin.h>

#if defined(_M_AMD64) || defined(__amd64__)
#define AMD64 1
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace mhdl::utils {

#ifdef _MSC_VER
inline unsigned truncLog2(unsigned v) {    
    return 31-__lzcnt(v);
}
#else
inline unsigned truncLog2(unsigned v) {    
    return 31-__builtin_clz(v);
}
#endif

inline unsigned nextPow2(unsigned v) 
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
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
    return _andn_u64(a, b);
}
#endif
template<>
inline std::uint32_t andNot(std::uint32_t a, std::uint32_t b) {
    return _andn_u32(a, b);
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
inline T bitMaskRange(unsigned start, unsigned count) {
    return ((T{ 1 } << count) - 1) << start;
}

template<typename T>
inline T bitfieldExtract(T a, unsigned start, unsigned count) {
    start &= 0xFF;
    count &= 0xFF;
    return (a >> start) & ((T{ 1 } << count) - 1);
}

#ifdef __BMI__
#ifdef AMD64
template<>
inline std::uint64_t bitfieldExtract(std::uint64_t a, unsigned start, unsigned count) {
    return _bextr_u64(a, start, count);
}
#endif

template<>
inline std::uint32_t bitfieldExtract(std::uint32_t a, unsigned start, unsigned count) {
    return _bextr_u32(a, start, count);
}
#endif

template<typename T>
inline T bitfieldInsert(T a, unsigned start, unsigned count, T v) {
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
    
}
    
