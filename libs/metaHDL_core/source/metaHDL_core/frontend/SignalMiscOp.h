#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Multiplexer.h"
#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"


#include <boost/format.hpp>


namespace mhdl::core::frontend {
    
///@todo overload for compound signals
template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>    
SignalType mux(const Bit &selector, const SignalType &lhs, const SignalType &rhs)  {                                 
    hlim::Node_Signal *lhsSignal = lhs.getNode();
    hlim::Node_Signal *rhsSignal = rhs.getNode();
    MHDL_ASSERT(lhsSignal != nullptr);
    MHDL_ASSERT(rhsSignal != nullptr);
    MHDL_DESIGNCHECK_HINT(lhsSignal->getConnectionType() == rhsSignal->getConnectionType(), "Can only multiplex operands of same type (e.g. width).");
    
    hlim::Node_Multiplexer *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Multiplexer>(2);
    node->recordStackTrace();
    selector.getNode()->getOutput(0).connect(node->getInput(0));
    lhsSignal->getOutput(0).connect(node->getInput(1));
    rhsSignal->getOutput(0).connect(node->getInput(2));

    return SignalType(&node->getOutput(0), lhsSignal->getConnectionType());
}

///@todo overload for compound signals
///@todo doesn't work yet
template<typename SelectorType, typename ContainerType, typename SignalType = typename ContainerType::value_type, typename = std::enable_if_t<
                                                                                                utils::isElementarySignal<SignalType>::value &&
                                                                                                utils::isUnsignedIntegerSignal<SelectorType>::value &&
                                                                                                utils::isContainer<ContainerType>::value
                                                                                            >>    
SignalType mux(const SelectorType &selector, const ContainerType &inputs)  {                                 
    MHDL_DESIGNCHECK_HINT(!inputs.empty(), "Inputs can not be empty");
    
    const hlim::Node_Signal *firstSignal = inputs.begin()->getNode();
    MHDL_ASSERT(firstSignal != nullptr);
    
    hlim::Node_Multiplexer *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Multiplexer>(inputs.size());
    node->recordStackTrace();
    selector.getNode()->getOutput(0).connect(node->getInput(0));
    for (const auto &pair : ConstEnumerate(inputs)) {
        const hlim::Node_Signal *thisSignal = pair.second.getNode();
        MHDL_ASSERT(thisSignal != nullptr);
        MHDL_DESIGNCHECK_HINT(firstSignal->getConnectionType() == thisSignal->getConnectionType(), "Can only multiplex operands of same type (e.g. width).");

        pair.second.getNode()->getOutput(0).connect(node->getInput(pair.first+1));
    }
    
    return SignalType(&node->getOutput(0), firstSignal->getConnectionType());
}




///@todo overload for compound signals
template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>    
void driveWith(SignalType &dst, const SignalType &src)  {                                 
    hlim::Node_Signal *signalNode = dst.getNode();
    MHDL_ASSERT(signalNode != nullptr);
    
    MHDL_DESIGNCHECK_HINT(signalNode->getInput(0).connection == nullptr, "Signal is already being driven.");
    
    MHDL_ASSERT(src.getNode() != nullptr);
    src.getNode()->getOutput(0).connect(signalNode->getInput(0));
    
    signalNode->moveToGroup(Scope::getCurrentNodeGroup());
}



//cat(....)



}
