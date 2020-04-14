#pragma once
#include "../Node.h"

namespace mhdl {
namespace core {    
namespace hlim {
    
class Node_Arithmetic : public Node
{
    public:
        enum Op {
            ADD,
            SUB,
            MUL,
            DIV,
            REM
        };
        
        Node_Arithmetic(NodeGroup *group, Op op) : Node(group, 2, 1), m_op(op) { }
        
        virtual std::string getTypeName() const override { return "Arithmetic"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(unsigned idx) const override { return idx==0?"a":"b"; }
        virtual std::string getOutputName(unsigned idx) const override { return "out"; }

        inline Op getOp() const { return m_op; }
    protected:
        Op m_op;
        // extend or not, etc...
};

}
}
}
