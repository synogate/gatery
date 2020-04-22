#pragma once
#include "../Node.h"

namespace mhdl::core::hlim {
    
class Node_Logic : public Node<Node_Logic>
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
        
        Node_Logic(Op op);
        
        void connectInput(size_t operand, const NodePort &port);
        void disconnectInput(size_t operand);
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        inline Op getOp() const { return m_op; }
    protected:
        Op m_op;
        
        void updateConnectionType();        
};

}
