#pragma once

#include "../Node.h"
#include "../ConnectionType.h"

namespace mhdl::core::hlim {
    
class Node_Signal : public Node<Node_Signal>
{
    public:
        Node_Signal() : Node(1, 1) {  }
        
        virtual std::string getTypeName() const override { return "Signal"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override { return "in"; }
        virtual std::string getOutputName(size_t idx) const override { return "out"; }
        
        void setConnectionType(const ConnectionType &connectionType);
        
        void connectInput(const NodePort &nodePort);
        void disconnectInput();
};

}
