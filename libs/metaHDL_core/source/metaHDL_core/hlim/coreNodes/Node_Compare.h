#pragma once
#include "../Node.h"

namespace mhdl {
namespace core {    
namespace hlim {
    
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
        
        virtual std::string getTypeName() override { return "Compare"; }
        virtual void assertValidity() override { }
        virtual std::string getInputName(unsigned idx) override { return idx==0?"a":"b"; }
        virtual std::string getOutputName(unsigned idx) override { return "out"; }
    protected:
};

}
}
}
