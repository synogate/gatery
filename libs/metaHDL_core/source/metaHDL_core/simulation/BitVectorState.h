#pragma once

#include "../utils/BitManipulation.h"
#include "../utils/Range.h"

#include <vector>
#include <array>
#include <cstdint>
#include <string.h>

namespace mhdl::core::sim {

struct DefaultConfig
{
    using BaseType = std::uint64_t;
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
        
        bool get(typename Config::Plane plane, size_t idx);
        void set(typename Config::Plane plane, size_t idx);
        void set(typename Config::Plane plane, size_t idx, bool bit);
        void clear(typename Config::Plane plane, size_t idx);
        void toggle(typename Config::Plane plane, size_t idx);
        
        typename Config::BaseType *data(typename Config::Plane plane);
        const typename Config::BaseType *data(typename Config::Plane plane) const;
        
        BitVectorState<Config> extract(size_t start, size_t size);
        
        typename Config::BaseType extractNonStraddling(typename Config::Plane plane, size_t start, size_t size);
        void insertNonStraddling(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value);
    protected:
        size_t m_size;
        std::array<std::vector<std::uint64_t>, Config::NUM_PLANES> m_values;
};

class DefaultBitVectorState : public BitVectorState<DefaultConfig>
{
    public:
        bool allDefinedNonStraddling(size_t start, size_t size) {
            return utils::andNot(extractNonStraddling(DefaultConfig::DEFINED, start, size), utils::bitMaskRange(0, size));
        }
};









template<class Config>
void BitVectorState<Config>::resize(size_t size)
{
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
bool BitVectorState<Config>::get(typename Config::Plane plane, size_t idx)
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
BitVectorState<Config> BitVectorState<Config>::extract(size_t start, size_t size)
{
    BitVectorState<Config> result;
    result.resize(size);
    if (start % 8 == 0) {
        for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
            memcpy((char*) result.data(i), (char*) data(i) + start/8, (size+7)/8);
    } else {
        ///@todo optimize
        for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
            for (auto j : utils::Range(size))
                result.set(i, j, get(i, j+start));
    }
    return result;
}

template<class Config>
typename Config::BaseType BitVectorState<Config>::extractNonStraddling(typename Config::Plane plane, size_t start, size_t size)
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
