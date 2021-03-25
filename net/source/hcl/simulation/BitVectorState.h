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
        class iterator
        {
        public:
            class proxy
            {
            public:
                proxy(BitVectorState& state, typename Config::Plane plane, size_t offset, size_t size) :
                    m_state(state), m_plane(plane), m_offset(offset), m_size(size) {}

                operator typename Config::BaseType() const { return m_state.extract(m_plane, m_offset, m_size); }
                proxy& operator = (typename Config::BaseType value) { m_state.insert(m_plane, m_offset, m_size, value); return *this; }

            private:
                BitVectorState& m_state;
                const typename Config::Plane m_plane;
                const size_t m_offset;
                const size_t m_size;
            };

            iterator(BitVectorState& state, typename Config::Plane plane, size_t offset, size_t end) :
                m_state(state), m_plane(plane), m_offset(offset), m_end(end) {}

            bool operator != (const iterator& r) const { return m_offset != r.m_offset; }
            bool operator == (const iterator& r) const { return m_offset == r.m_offset; }

            iterator& operator ++() { m_offset += stepWidth(); return *this; }
            iterator operator ++(int) { return iterator{m_state, m_plane, m_offset + stepWidth(), m_end}; }

            proxy operator *() { return proxy{ m_state, m_plane, m_offset, stepWidth() }; }

            size_t stepWidth() const { return std::min(sizeof(typename Config::BaseType) * 8, m_end - m_offset); }
            size_t mask() const { return (1ull << stepWidth()) - 1; }

        private:
            BitVectorState& m_state;
            const typename Config::Plane m_plane;
            size_t m_offset;
            const size_t m_end;
        };

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
        void insert(const BitVectorState& state, size_t offset);

        typename Config::BaseType extract(typename Config::Plane plane, size_t offset, size_t size) const;
        typename Config::BaseType extractNonStraddling(typename Config::Plane plane, size_t start, size_t size) const;

        void insert(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value);
        void insertNonStraddling(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value);

        std::pair<iterator, iterator> range(typename Config::Plane plane, size_t offset, size_t size);

protected:
        size_t m_size = 0;
        std::array<std::vector<typename Config::BaseType>, Config::NUM_PLANES> m_values;
};


template<typename Config>
bool allDefinedNonStraddling(const BitVectorState<Config> &vec, size_t start, size_t size) {
    return !utils::andNot(vec.extractNonStraddling(Config::DEFINED, start, size), utils::bitMaskRange(0, size));
}




using DefaultBitVectorState = BitVectorState<DefaultConfig>;

template<typename Config>
std::ostream& operator << (std::ostream& s, const BitVectorState<Config>& state)
{
    if ((s.flags() & std::ios_base::hex) && (state.size() % 4 == 0)) {
        for (auto i : utils::Range(state.size()/4)) {
            bool allDefined = true;
            unsigned v = 0;
            for (auto j : utils::Range(4)) {
                v <<= 1;
                allDefined  &= state.get(Config::DEFINED, state.size() - 1 - i*4-j);

                if (state.get(Config::VALUE, state.size() - 1 - i*4-j))
                    v |= 1;
            }
            s << v;
        }
        return s;
    } else {
        for (size_t i = state.size()-1; i < state.size(); --i)
        {
            if (!state.get(Config::DEFINED, i))
                s << 'X';
            else if (state.get(Config::VALUE, i))
                s << '1';
            else
                s << '0';
        }
        return s;
    }
}




template<typename Config>
void formatRange(std::ostream& s, const BitVectorState<Config>& state, unsigned base, size_t offset, size_t size)
{
    unsigned logBase = utils::Log2C(base);
    //HCL_ASSERT(size % logBase == 0);
    size_t roundUpSize = (size+logBase-1)/logBase*logBase;

    for (auto i : utils::Range(roundUpSize/logBase)) {
        bool allDefined = true;
        unsigned v = 0;
        for (auto j : utils::Range(logBase)) {
            v <<= 1;
            auto idx = roundUpSize - 1 - i*logBase-j;
            if (idx < size) {
                allDefined  &= state.get(Config::DEFINED, offset + idx);

                if (state.get(Config::VALUE, offset + idx))
                    v |= 1;
            }
        }
        if (!allDefined)
            s << 'X';
        else
            if (v < 10)
                s << (unsigned) v;
            else
                s << (char)('A' + (v-10));
    }
}

template<typename Config, typename Functor>
BitVectorState<Config> createBitVectorState(std::size_t numWords, std::size_t wordSize, Functor functor) {
    BitVectorState<Config> state;
    state.resize(numWords * wordSize);

    for (auto i : utils::Range(numWords)) {
        typename Config::BaseType planes[Config::NUM_PLANES];

        functor(i, planes);

        for (auto p : utils::Range(Config::NUM_PLANES))
            if (sizeof(typename Config::BaseType) % wordSize == 0)
                state.insertNonStraddling(p, i * wordSize, wordSize, planes[p]);
            else
                state.insert(p, i * wordSize, wordSize, planes[p]);
    }

    return state;
}

template<typename Functor>
DefaultBitVectorState createDefaultBitVectorState(std::size_t numWords, std::size_t wordSize, Functor functor) {
    return createBitVectorState<DefaultConfig, Functor>(numWords, wordSize, functor);
}



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
    if (srcOffset % 8 == 0 && dstOffset % 8 == 0 && size >= 8) {
        size_t bytes = size / 8;
        for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
            memcpy((char*) data((typename Config::Plane) i) + dstOffset/8, (const char*) src.data((typename Config::Plane) i) + srcOffset/8, bytes);

        dstOffset += bytes * 8;
        srcOffset += bytes * 8;
        size -= bytes*8;
    }

    ///@todo: Optimize aligned cases (which happen quite frequently!)
    size_t width = size;
    size_t offset = 0;
    while (offset < width) {
        size_t chunkSize = std::min<size_t>(Config::NUM_BITS_PER_BLOCK, width-offset);

        for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
            insert((typename Config::Plane) i, dstOffset + offset, chunkSize,
                    (typename Config::BaseType) src.extract((typename Config::Plane) i, srcOffset + offset, chunkSize));

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
inline void BitVectorState<Config>::insert(const BitVectorState& state, size_t offset)
{
    const size_t width = state.size();
    size_t srcOffset = 0;

    while (srcOffset < width) {
        size_t chunkSize = std::min<size_t>(64, width - srcOffset);

        auto val = state.extractNonStraddling(sim::DefaultConfig::VALUE, srcOffset, chunkSize);
        insertNonStraddling(sim::DefaultConfig::VALUE, offset, chunkSize, val);

        auto def = state.extractNonStraddling(sim::DefaultConfig::DEFINED, srcOffset, chunkSize);
        insertNonStraddling(sim::DefaultConfig::DEFINED, offset, chunkSize, def);

        offset += chunkSize;
        srcOffset += chunkSize;
    }
}

template<class Config>
inline typename Config::BaseType BitVectorState<Config>::extract(typename Config::Plane plane, size_t offset, size_t size) const
{
    HCL_ASSERT(size <= Config::NUM_BITS_PER_BLOCK);
    const auto* values = &m_values[plane][offset / Config::NUM_BITS_PER_BLOCK];
    const size_t wordOffset = offset % Config::NUM_BITS_PER_BLOCK;

    auto val = values[0];
    val >>= wordOffset;
    if(wordOffset + size > Config::NUM_BITS_PER_BLOCK)
        val |= values[1] << (Config::NUM_BITS_PER_BLOCK - wordOffset);
    val &= utils::bitMaskRange(0, size);

    return val;
}

template<class Config>
typename Config::BaseType BitVectorState<Config>::extractNonStraddling(typename Config::Plane plane, size_t start, size_t size) const
{
    HCL_ASSERT(start % Config::NUM_BITS_PER_BLOCK + size <= Config::NUM_BITS_PER_BLOCK);
    return utils::bitfieldExtract(m_values[plane][start / Config::NUM_BITS_PER_BLOCK], start % Config::NUM_BITS_PER_BLOCK, size);
}

template<class Config>
inline void BitVectorState<Config>::insert(typename Config::Plane plane, size_t offset, size_t size, typename Config::BaseType value)
{
    HCL_ASSERT(size <= Config::NUM_BITS_PER_BLOCK);
    const size_t wordOffset = offset % Config::NUM_BITS_PER_BLOCK;
    if (wordOffset + size <= Config::NUM_BITS_PER_BLOCK)
    {
        insertNonStraddling(plane, offset, size, value);
        return;
    }

    auto* dst = &m_values[plane][offset / Config::NUM_BITS_PER_BLOCK];
    dst[0] = utils::bitfieldInsert(dst[0], wordOffset, Config::NUM_BITS_PER_BLOCK - wordOffset, value);
    value >>= Config::NUM_BITS_PER_BLOCK - wordOffset;
    dst[1] = utils::bitfieldInsert(dst[1], 0, (wordOffset + size) % Config::NUM_BITS_PER_BLOCK, value);
}

template<class Config>
void BitVectorState<Config>::insertNonStraddling(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value)
{
    HCL_ASSERT(start % Config::NUM_BITS_PER_BLOCK + size <= Config::NUM_BITS_PER_BLOCK);
    if (size)
    {
        auto& op = m_values[plane][start / Config::NUM_BITS_PER_BLOCK];
        op = utils::bitfieldInsert(op, start % Config::NUM_BITS_PER_BLOCK, size, value);
    }
}

template<class Config>
inline std::pair<typename BitVectorState<Config>::iterator, typename BitVectorState<Config>::iterator>
BitVectorState<Config>::range(typename Config::Plane plane, size_t offset, size_t size)
{
    const size_t endOffset = offset + size;
    return {
        iterator{*this, plane, offset, endOffset},
        iterator{*this, plane, endOffset, endOffset}
    };
}



}
