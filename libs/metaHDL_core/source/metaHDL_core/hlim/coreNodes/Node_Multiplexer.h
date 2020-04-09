#pragma once

#include "../Node.h"

#include <boost/format.hpp>

namespace mhdl {
namespace core {    
namespace hlim {
    
class Node_Multiplexer : public Node
{
    public:
        Node_Multiplexer(NodeGroup *group, unsigned numMultiplexedInputs) : Node(group, 1+numMultiplexedInputs, 1) { }
        
        virtual std::string getTypeName() override { return "Multiplexer"; }
        virtual void assertValidity() override { }
        virtual std::string getInputName(unsigned idx) override { if (idx == 0) return "select"; return (boost::format("in_%d") % (idx-1)).str(); }
        virtual std::string getOutputName(unsigned idx) override { return "out"; }
};

}
}
}
