#pragma once

#include "Range.h"

#include <cstdint>

#include <immintrin.h>

namespace mhdl::utils {
    
    
    
#ifdef __BMI__
inline std::uint64_t andNot(std::uint64_t a, std::uint64_t b) {
    return _bandn_u64(a, b);
}
#else
inline std::uint64_t andNot(std::uint64_t a, std::uint64_t b) {
    return ~a & b;
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
    a = andNot(1ull << idx, a);
#else
    _bittestandreset64(&a, idx);
#endif
}

inline void bitClear(const void *a, size_t idx) {
#if 1
    std::uint64_t &v = ((std::uint64_t*)a)[idx/64];
    v = andNot(1ull << (idx % 64), v);
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


inline std::uint64_t bitMaskRange(unsigned start, unsigned count) {
    return (~0ull >> (64-count)) << start;
}


#ifdef __BMI__
inline std::uint64_t bitfieldExtract(std::uint64_t a, unsigned start, unsigned count) {
    return _bextr_u64(a, start, count);
}
#else
inline std::uint64_t bitfieldExtract(std::uint64_t a, unsigned start, unsigned count) {
    return (a >> start) & ((1ull << count) - 1);
}
#endif

inline std::uint64_t bitfieldInsert(std::uint64_t a, unsigned start, unsigned count, std::uint64_t v) {
    std::uint64_t mask = bitMaskRange(start, count);
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
    
