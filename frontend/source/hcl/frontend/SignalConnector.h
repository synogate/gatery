#pragma once

#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>

namespace hcl::core::frontend {

template<typename SignalType>
class SignalConnector
{
    public:
        SignalConnector(hlim::Node_Signal *signalNode);
        SignalConnector(SignalType &signal);
        SignalConnector(const SignalConnector<SignalType> &other) = default;
        SignalConnector<SignalType> &operator=(const SignalConnector<SignalType> &other) = default;
        
        void operator<=(const SignalType &driver);
    protected:
        hlim::Node_Signal *m_signalNode;
};



template<typename SignalType>
SignalConnector<SignalType>::SignalConnector(hlim::Node_Signal *signalNode) : m_signalNode(signalNode)
{
}


template<typename SignalType>
SignalConnector<SignalType>::SignalConnector(SignalType &signal)
{
    m_signalNode = signal.getNode();
}

template<typename SignalType>
void SignalConnector<SignalType>::operator<=(const SignalType &driver)
{
    ///@todo global enable mux / conditional scopes
    m_signalNode->connectInput(driver.getReadPort());
    m_signalNode->moveToGroup(GroupScope::getCurrentNodeGroup());
}



}
