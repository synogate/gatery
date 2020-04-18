#pragma once

#include "../Node.h"

#include <boost/format.hpp>

namespace mhdl {
namespace core {    
namespace hlim {
    
class Node_Register : public Node
{
    public:
        Node_Register(NodeGroup *group) : Node(group, 3, 1) { }
        
        virtual std::string getTypeName() const override { return "Register"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override { switch (idx) { case 0: return "in"; case 1: return "enable"; default: return "resetValue"; } }
        virtual std::string getOutputName(size_t idx) const override { return "out"; }
};

}
}
}
