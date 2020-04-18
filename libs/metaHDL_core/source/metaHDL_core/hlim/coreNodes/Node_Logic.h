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
        
        virtual std::string getTypeName() const override { return ""; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override { return ""; }
        virtual std::string getOutputName(size_t idx) const override { return "output"; }

        inline Op getOp() const { return m_op; }
    protected:
        Op m_op;
};

}
}
}
