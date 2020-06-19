#pragma once

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"
#include "../utils/BitManipulation.h"
#include "../utils/Range.h"

#include <vector>
#include <array>
#include <cstdint>
#include <string.h>

namespace hcl::core::sim {

struct DefaultConfig
{
    using BaseType = std::size_t;
    enum {
        NUM_BITS_PER_BLOCK = sizeof(BaseType)*8
    };
    enum Plane {
        VALUE,
        DEFINED,
        NUM_PLANES
    };
};

template<class Config>
class BitVectorState
{
    public:
        void resize(size_t size);
        inline size_t size() const { return m_size; }
        inline size_t getNumBlocks() const { return m_values[0].size(); }
        void clear();
        
        bool get(typename Config::Plane plane, size_t idx) const;
        void set(typename Config::Plane plane, size_t idx);
        void set(typename Config::Plane plane, size_t idx, bool bit);
        void clear(typename Config::Plane plane, size_t idx);
        void toggle(typename Config::Plane plane, size_t idx);

        void setRange(typename Config::Plane plane, size_t offset, size_t size, bool bit);
        void setRange(typename Config::Plane plane, size_t offset, size_t size);
        void clearRange(typename Config::Plane plane, size_t offset, size_t size);
        void copyRange(size_t dstOffset, const BitVectorState<Config> &src, size_t srcOffset, size_t size);
        
        typename Config::BaseType *data(typename Config::Plane plane);
        const typename Config::BaseType *data(typename Config::Plane plane) const;
        
        BitVectorState<Config> extract(size_t start, size_t size) const;
        
        typename Config::BaseType extractNonStraddling(typename Config::Plane plane, size_t start, size_t size) const;
        void insertNonStraddling(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value);
    protected:
        size_t m_size;
        std::array<std::vector<typename Config::BaseType>, Config::NUM_PLANES> m_values;
};


template<typename Config>
bool allDefinedNonStraddling(const BitVectorState<Config> &vec, size_t start, size_t size) {
    return !utils::andNot(vec.extractNonStraddling(Config::DEFINED, start, size), utils::bitMaskRange(0, size));
}

using DefaultBitVectorState = BitVectorState<DefaultConfig>;








template<class Config>
void BitVectorState<Config>::resize(size_t size)
{
    m_size = size;
    for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
        m_values[i].resize((size+Config::NUM_BITS_PER_BLOCK-1) / Config::NUM_BITS_PER_BLOCK);
}

template<class Config>
void BitVectorState<Config>::clear()
{
    for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
        m_values[i].clear();
}

template<class Config>
bool BitVectorState<Config>::get(typename Config::Plane plane, size_t idx) const
{
    return utils::bitExtract(m_values[plane].data(), idx);
}

template<class Config>
void BitVectorState<Config>::set(typename Config::Plane plane, size_t idx)
{
    utils::bitSet(m_values[plane].data(), idx);
}

template<class Config>
void BitVectorState<Config>::set(typename Config::Plane plane, size_t idx, bool bit)
{
    if (bit)
        utils::bitSet(m_values[plane].data(), idx);
    else
        utils::bitClear(m_values[plane].data(), idx);
}

template<class Config>
void BitVectorState<Config>::clear(typename Config::Plane plane, size_t idx)
{
    utils::bitClear(m_values[plane].data(), idx);
}

template<class Config>
void BitVectorState<Config>::toggle(typename Config::Plane plane, size_t idx)
{
    utils::bitToggle(m_values[plane].data(), idx);
}


template<class Config>
void BitVectorState<Config>::setRange(typename Config::Plane plane, size_t offset, size_t size, bool bit)
{
    typename Config::BaseType content = 0;
    if (bit)
        content = ~content;
    
    size_t firstWordSize;
    size_t wordOffset = offset / Config::NUM_BITS_PER_BLOCK;
    if (offset % Config::NUM_BITS_PER_BLOCK == 0) {
        firstWordSize = 0;
    } else {
        firstWordSize = std::min(size, Config::NUM_BITS_PER_BLOCK - offset % Config::NUM_BITS_PER_BLOCK);
        insertNonStraddling(plane, offset, firstWordSize, content);
        wordOffset++;
    }
    
    size_t numFullWords = (size - firstWordSize) / Config::NUM_BITS_PER_BLOCK;
    for (auto i : utils::Range(numFullWords))
        m_values[plane][wordOffset + i] = content;


    size_t trailingWordSize = (size - firstWordSize) % Config::NUM_BITS_PER_BLOCK;
    if (trailingWordSize > 0)
        insertNonStraddling(plane, offset + firstWordSize + numFullWords*Config::NUM_BITS_PER_BLOCK, trailingWordSize, content);
}

template<class Config>
void BitVectorState<Config>::setRange(typename Config::Plane plane, size_t offset, size_t size)
{
    setRange(plane, offset, size, true);
}

template<class Config>
void BitVectorState<Config>::clearRange(typename Config::Plane plane, size_t offset, size_t size)
{
    setRange(plane, offset, size, false);
}

template<class Config>
void BitVectorState<Config>::copyRange(size_t dstOffset, const BitVectorState<Config> &src, size_t srcOffset, size_t size)
{
    // This code assumes that either offsets are aligned to block boundaries, or the access doesn't cross block boundaries    
    HCL_ASSERT(dstOffset % Config::NUM_BITS_PER_BLOCK == 0 || 
                dstOffset % Config::NUM_BITS_PER_BLOCK + size <= Config::NUM_BITS_PER_BLOCK);

    HCL_ASSERT(srcOffset % Config::NUM_BITS_PER_BLOCK == 0 || 
                (srcOffset % Config::NUM_BITS_PER_BLOCK) + size <= Config::NUM_BITS_PER_BLOCK);
    
    
    ///@todo: Optimize aligned cases (which happen quite frequently!)
    size_t width = size;
    size_t offset = 0;
    while (offset < width) {
        size_t chunkSize = std::min<size_t>(Config::NUM_BITS_PER_BLOCK, width-offset);
        
        for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
            insertNonStraddling((typename Config::Plane) i, dstOffset + offset, chunkSize,
                    src.extractNonStraddling((typename Config::Plane) i, srcOffset+offset, chunkSize));

        offset += chunkSize;
    }

}


template<class Config>
typename Config::BaseType *BitVectorState<Config>::data(typename Config::Plane plane)
{
    return m_values[plane].data();
}

template<class Config>
const typename Config::BaseType *BitVectorState<Config>::data(typename Config::Plane plane) const
{
    return m_values[plane].data();
}

template<class Config>
BitVectorState<Config> BitVectorState<Config>::extract(size_t start, size_t size) const
{
    BitVectorState<Config> result;
    result.resize(size);
    if (start % 8 == 0) {
        for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
            memcpy((char*) result.data((typename Config::Plane) i), (char*) data((typename Config::Plane) i) + start/8, (size+7)/8);
    } else
        result.copyRange(0, *this, start, size);

    return result;
}

template<class Config>
typename Config::BaseType BitVectorState<Config>::extractNonStraddling(typename Config::Plane plane, size_t start, size_t size) const
{
    return utils::bitfieldExtract(m_values[plane][start / Config::NUM_BITS_PER_BLOCK], start % Config::NUM_BITS_PER_BLOCK, size);
}

template<class Config>
void BitVectorState<Config>::insertNonStraddling(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value)
{
    auto &op = m_values[plane][start / Config::NUM_BITS_PER_BLOCK];
    op = utils::bitfieldInsert(op, start % Config::NUM_BITS_PER_BLOCK, size, value);
}



}
