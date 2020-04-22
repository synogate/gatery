#pragma once

#include "../Node.h"

#include <boost/format.hpp>

namespace mhdl::core::hlim {
    
class Node_Register : public Node
{
    public:
        enum Input {
            DATA,
            RESET_VALUE,
            ENABLE,
            NUM_INPUTS
        };
        
        Node_Register();
        
        inline void connectInput(Input input, const NodePort &port) { NodeIO::connectInput(input, port); }
        inline void disconnectInput(Input input) { NodeIO::disconnectInput(input); }
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;
};

}
