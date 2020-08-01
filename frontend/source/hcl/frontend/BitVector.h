#pragma once

#include "Bit.h"
#include "Signal.h"
#include "SignalPort.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>

#include <hcl/utils/Exceptions.h>

#include <vector>
#include <algorithm>
#include <compare>


namespace hcl::core::frontend {

// concat x = a && b && c;
    
    
struct Selection {
    int start = 0;
    int end = 0;
    int stride = 1;
    bool untilEndOfSource = false;    
    
    static Selection From(int start);
    static Selection Range(int start, int end);
    static Selection RangeIncl(int start, int endIncl);
    static Selection StridedRange(int start, int end, int stride);

    static Selection Slice(int offset, size_t size);
    static Selection StridedSlice(int offset, size_t size, int stride);
};


class BVec;

class BVecSlice
{
    public:
        using isBitVectorSignalLike = void;
        
        ~BVecSlice();
        
        BVecSlice &operator=(const BVecSlice &slice);
        BVecSlice &operator=(const ElementarySignal &signal);
        operator BVec() const;
        
        size_t size() const;
    protected:
        BVecSlice() = delete;
        BVecSlice(const BVecSlice &) = delete;
        BVecSlice(BVec *signal, const Selection &selection);
        
        
        BVec *m_signal;
        Selection m_selection;
        
        hlim::NodePort m_lastSignalNodePort;
        
        friend class BVec;
        
        void unregisterSignal();
};

template<typename TVec>
class BVecBitProxy
{
public:
    BVecBitProxy(TVec* vec, size_t index) : m_vec{ vec }, m_index{ index } {}

    BVecBitProxy& operator = (BitSignalPort value) { m_vec->setBit(m_index, Bit{ value }); return *this; }
    operator Bit () const { return (*(const TVec*)m_vec)[m_index]; };

    BVec zext(size_t width) const;
    BVec sext(size_t width) const;
    BVec bext(size_t width, const Bit& bit) const;

private:
    TVec* const m_vec;
    const size_t m_index;
};

template<typename TVec>
class BVecIterator
{
public:

public:
    BVecIterator(TVec* vec, size_t index) : m_vec{vec}, m_index{index} {}
    BVecIterator(const BVecIterator&) = default;

    BVecIterator& operator ++ () { ++m_index; return *this; }
    BVecIterator operator ++ (int) { BVecIterator tmp = *this; m_index++; return tmp; }
    BVecIterator& operator -- () { --m_index; return *this; }
    BVecIterator operator -- (int) { BVecIterator tmp = *this; m_index--; return tmp; }
    BVecIterator operator + (size_t offset) const { return { m_vec, m_index + offset }; }
    BVecIterator operator - (size_t offset) const { return { m_vec, m_index - offset }; }
    BVecIterator& operator += (size_t offset) { m_index += offset; return *this; }
    BVecIterator& operator -= (size_t offset) { m_index -= offset; return *this; }
    ptrdiff_t operator - (const BVecIterator& rhs) const { return ptrdiff_t(m_index) - rhs.m_index; }

    //bool operator != (const BVecIterator&) const = default;
    auto operator <=> (const BVecIterator& rhs) const = default; // { assert(m_vec == rhs.m_vec); return m_index <=> rhs.m_index; }
    BVecBitProxy<TVec> operator* () const { return { m_vec, m_index }; }

private:
    TVec* m_vec;
    size_t m_index;
};

class BVec : public ElementarySignal
{
    public:
        using PortType = BVecSignalPort;

        HCL_SIGNAL
        using ElementarySignal::ElementarySignal;
        using isBitVectorSignal = void;

        BVec();
        BVec(size_t width);
        BVec(const hlim::NodePort &port);
        BVec(BVecSignalPort rhs);
        BVec(const BVec& rhs) : BVec{ BVecSignalPort{rhs} } {}
        ~BVec();

        BVec zext(size_t width) const;
        BVec sext(size_t width) const { return bext(width, msb()); }
        BVec bext(size_t width, const Bit& bit) const;

        BVecSlice operator()(int offset, size_t size) { return BVecSlice(this, Selection::Slice(offset, size)); }
        BVecSlice operator()(const Selection &selection) { return BVecSlice(this, selection); }

        BVec& operator=(BVecSignalPort rhs) { assign(rhs); return *this; }
        BVec& operator=(const BVec& rhs) { assign(BVecSignalPort{ rhs }); return *this; }
        
        const BVec operator*() const;

        virtual void resize(size_t width);

        Bit operator[](size_t idx) const;
        BVecBitProxy<BVec> operator[](size_t idx) { assert(idx < size()); return { this, idx }; }

        void setBit(size_t idx, BitSignalPort in);

        Bit lsb() const { return (*this)[0]; }
        Bit msb() const { return (*this)[size()-1]; }

        bool empty() const { return size() == 0; }
        size_t size() const { return getWidth(); }

        Bit front() const { return lsb(); }
        Bit back() const { return msb(); }
        BVecBitProxy<BVec> front() { return { this, 0 }; }
        BVecBitProxy<BVec> back() { return { this, size() - 1 }; }

        BVecIterator<BVec> begin() { return { this, 0 }; }
        BVecIterator<BVec> end() { return { this, size() }; }
        BVecIterator<const BVec> cbegin() const { return { this, 0 }; }
        BVecIterator<const BVec> cend() const { return { this, size() }; }

    protected:
        BVec(const BVec &rhs, ElementarySignal::InitSuccessor);
        
        std::set<BVecSlice*> m_slices;
        friend class BVecSlice;
        void unregisterSlice(BVecSlice *slice) { m_slices.erase(slice); }

        virtual hlim::ConnectionType getSignalType(size_t width) const override;
};

template<typename TVec>
inline BVec BVecBitProxy<TVec>::zext(size_t width) const
{
    return ((Bit)*this).zext(width);
}

template<typename TVec>
inline BVec BVecBitProxy<TVec>::sext(size_t width) const
{
    return ((Bit)*this).sext(width);
}

template<typename TVec>
inline BVec BVecBitProxy<TVec>::bext(size_t width, const Bit& bit) const
{
    return ((Bit)*this).bext(width, bit);
}

}
