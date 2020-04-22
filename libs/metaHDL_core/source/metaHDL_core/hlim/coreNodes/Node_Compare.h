#pragma once
#include "../Node.h"

namespace mhdl::core::hlim {
    
class Node_Compare : public Node
{
    public:
        enum Op {
            EQ,
            NEQ,
            LT,
            GT,
            LEQ,
            GEQ
        };
        
        inline void connectInput(size_t operand, const NodePort &port) { NodeIO::connectInput(operand, port); }
        inline void disconnectInput(size_t operand) { NodeIO::disconnectInput(operand); }        
        
        virtual std::string getTypeName() const override { return "Compare"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override { return idx==0?"a":"b"; }
        virtual std::string getOutputName(size_t idx) const override { return "out"; }
    protected:
};

}
