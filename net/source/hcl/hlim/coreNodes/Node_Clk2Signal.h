#pragma once
#include "../Node.h"

namespace hcl::core::hlim {
    
class Node_Clk2Signal : public Node<Node_Clk2Signal>
{
    public:
        Node_Clk2Signal();
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        void setClock(Clock *clk);
    protected:
};

}
