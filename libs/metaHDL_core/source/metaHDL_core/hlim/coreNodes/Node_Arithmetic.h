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
        
        virtual std::string getTypeName() override { return "Arithmetic"; }
        virtual void assertValidity() override { }
        virtual std::string getInputName(unsigned idx) override { return idx==0?"a":"b"; }
        virtual std::string getOutputName(unsigned idx) override { return "out"; }
    protected:
        Op m_op;
        // extend or not, etc...
};

}
}
}
