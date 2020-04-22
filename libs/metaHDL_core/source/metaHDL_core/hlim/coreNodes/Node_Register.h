#pragma once

#include "../Node.h"

#include <boost/format.hpp>

namespace mhdl::core::hlim {
    
class Node_Register : public Node<Node_Register>
{
    public:
        enum Input {
            DATA,
            RESET_VALUE,
            ENABLE,
            NUM_INPUTS
        };
        
        Node_Register();
        
        void connectInput(Input input, const NodePort &port);
        inline void disconnectInput(Input input) { NodeIO::disconnectInput(input); }
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;
};

}
