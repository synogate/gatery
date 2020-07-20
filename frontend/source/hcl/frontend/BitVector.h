#pragma once

#include "Bit.h"
#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>

#include <hcl/utils/Exceptions.h>

#include <vector>
#include <algorithm>


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


class BVec : public ElementarySignal
{
    public:
        HCL_SIGNAL
        using ElementarySignal::ElementarySignal;
        using isBitVectorSignal = void;

        BVec() = default;
        BVec(size_t width);
        BVec(const hlim::NodePort &port);

        BVec(const BVec &rhs) : ElementarySignal(rhs.getReadPort()) {}
        ~BVec() { for (auto slice : m_slices) slice->unregisterSignal(); }

        BVec zext(size_t width) const;
        BVec sext(size_t width) const { return bext(width, msb()); }
        BVec bext(size_t width, const Bit& bit) const;

        BVecSlice operator()(int offset, size_t size) { return BVecSlice(this, Selection::Slice(offset, size)); }
        BVecSlice operator()(const Selection &selection) { return BVecSlice(this, selection); }

        BVec &operator=(const BVec &rhs) { assign(rhs); return *this; }

        virtual void resize(size_t width);

        Bit operator[](size_t idx) const;

        void setBit(size_t idx, const Bit& in);

        Bit lsb() const { return (*this)[0]; }
        Bit msb() const { return (*this)[getWidth()-1]; }

        SignalConnector<BVec> getPrimordial() { return ElementarySignal::getPrimordial<BVec>(); }
    protected:
        std::set<BVecSlice*> m_slices;
        friend class BVecSlice;
        void unregisterSlice(BVecSlice *slice) { m_slices.erase(slice); }

        virtual hlim::ConnectionType getSignalType(size_t width) const override;
};

        

inline SignalConnector<BVec> primordial(BVec &bvec) { return bvec.getPrimordial(); }


}
