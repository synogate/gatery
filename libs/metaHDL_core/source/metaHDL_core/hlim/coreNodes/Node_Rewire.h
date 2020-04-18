#pragma once
#include "../Node.h"

#include <boost/format.hpp>

#include <vector>

namespace mhdl::core::hlim {
    
class Node_Rewire : public Node
{
    public:
        struct OutputRange {
            size_t subwidth;
            enum Source {
                INPUT,
                CONST_ZERO,
                CONST_ONE,
            };
            Source source;
            size_t inputIdx;
            size_t inputOffset;
        };
        
        struct RewireOperation {
            std::vector<OutputRange> ranges;
            
            bool isBitExtract(size_t& bitIndex) const;
            bool isRegularBitShift(int &shift, bool &keepSign) const;
        };
        
        Node_Rewire(NodeGroup *group, size_t numInputs);
        
        virtual std::string getTypeName() const override { return "Rewire"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override { return (boost::format("in_%d")%idx).str(); }
        virtual std::string getOutputName(size_t idx) const override { return "output"; }
        
        void setConcat();
        void setInterleave();
        void setExtract(size_t offset, size_t count, size_t stride = 1);        
        
        inline void setOp(RewireOperation rewireOperation) { m_rewireOperation = std::move(rewireOperation); }
        inline const RewireOperation &getOp() const { return m_rewireOperation; }
    protected:
        RewireOperation m_rewireOperation;
};

}
