#pragma once

#include "../Node.h"
#include "../ConnectionType.h"

namespace mhdl {
namespace core {    
namespace hlim {
    
class Node_Signal : public Node
{
    public:
        Node_Signal(NodeGroup *group) : Node(group, 1, 1) {  }
        
        virtual std::string getTypeName() const override { return "Signal"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(unsigned idx) const override { return "in"; }
        virtual std::string getOutputName(unsigned idx) const override { return "out"; }
        
        void setConnectionType(const ConnectionType &connectionType);
        inline const ConnectionType &getConnectionType() const { return m_connectionType; }
    protected:
        ConnectionType m_connectionType;
};

}
}
}
