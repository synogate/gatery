#pragma once
#include "../Node.h"

#include <boost/format.hpp>

#include <vector>

namespace mhdl {
namespace core {    
namespace hlim {
    
class Node_Rewire : public Node
{
    public:
        struct OutputRange {
            unsigned subwidth;
            enum Source {
                INPUT,
                CONST_ZERO,
                CONST_ONE,
            };
            Source source;
            unsigned inputIdx;
            unsigned inputOffset;
        };
        
        struct RewireOperation {
            std::vector<OutputRange> ranges;
        };
        
        Node_Rewire(NodeGroup *group, unsigned numInputs);
        
        virtual std::string getTypeName() const override { return "Rewire"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(unsigned idx) const override { return (boost::format("in_%d")%idx).str(); }
        virtual std::string getOutputName(unsigned idx) const override { return "output"; }
        
        void setConcat();
        void setInterleave();
        void setExtract(unsigned offset, unsigned count, unsigned stride = 1);        
        
        inline void setOp(RewireOperation rewireOperation) { m_rewireOperation = std::move(rewireOperation); }
    protected:
        RewireOperation m_rewireOperation;
};

}
}
}
