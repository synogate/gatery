#pragma once

#include "../Node.h"

#include <boost/format.hpp>

namespace mhdl {
namespace core {    
namespace hlim {
    
class Node_Multiplexer : public Node
{
    public:
        Node_Multiplexer(NodeGroup *group, size_t numMultiplexedInputs) : Node(group, 1+numMultiplexedInputs, 1) { }
        
        virtual std::string getTypeName() const override { return "Multiplexer"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override { if (idx == 0) return "select"; return (boost::format("in_%d") % (idx-1)).str(); }
        virtual std::string getOutputName(size_t idx) const override { return "out"; }
};

}
}
}
