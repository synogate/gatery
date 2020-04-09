#pragma once
#include "../Node.h"

namespace mhdl {
namespace core {    
namespace hlim {
    
class Node_Logic : public Node
{
    public:
        enum Op {
            AND,
            NAND,
            OR,
            NOR,
            XOR,
            EQ,
            NOT
        };
        
        Node_Logic(NodeGroup *group, Op op) : Node(group, op==NOT?1:2, 1), m_op(op) { }
        
        virtual std::string getTypeName() override { return ""; }
        virtual void assertValidity() override { }
        virtual std::string getInputName(unsigned idx) override { return ""; }
        virtual std::string getOutputName(unsigned idx) override { return "output"; }
    protected:
        Op m_op;
};

}
}
}
