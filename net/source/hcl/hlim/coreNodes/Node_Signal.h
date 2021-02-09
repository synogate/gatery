#pragma once

#include "../Node.h"
#include "../ConnectionType.h"

namespace hcl::core::hlim {

class SignalGroup;

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

        const SignalGroup *getSignalGroup() const { return m_signalGroup; }
        SignalGroup *getSignalGroup() { return m_signalGroup; }
        
        void moveToSignalGroup(SignalGroup *group);

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
    protected:
        SignalGroup *m_signalGroup = nullptr;
};

}
