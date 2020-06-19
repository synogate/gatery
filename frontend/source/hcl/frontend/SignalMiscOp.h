#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Multiplexer.h>
#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>


#include <boost/format.hpp>


namespace hcl::core::frontend {
    
///@todo overload for compound signals
template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>    
SignalType mux(const Bit &selector, const SignalType &lhs, const SignalType &rhs)  {                                 
    hlim::Node_Signal *lhsSignal = lhs.getNode();
    hlim::Node_Signal *rhsSignal = rhs.getNode();
    HCL_ASSERT(lhsSignal != nullptr);
    HCL_ASSERT(rhsSignal != nullptr);
    HCL_DESIGNCHECK_HINT(lhsSignal->getOutputConnectionType(0) == rhsSignal->getOutputConnectionType(0), "Can only multiplex operands of same type (e.g. width).");
    
    hlim::Node_Multiplexer *node = DesignScope::createNode<hlim::Node_Multiplexer>(2);
    node->recordStackTrace();
    node->connectSelector({.node = selector.getNode(), .port = 0ull});
    node->connectInput(0, {.node = lhsSignal, .port = 0ull});
    node->connectInput(1, {.node = rhsSignal, .port = 0ull});

    return SignalType({.node = node, .port = 0ull});
}

///@todo overload for compound signals
///@todo doesn't work yet
#if 0
template<typename SelectorType, typename ContainerType, typename SignalType = typename ContainerType::value_type, typename = std::enable_if_t<
                                                                                                utils::isElementarySignal<SignalType>::value &&
                                                                                                utils::isUnsignedIntegerSignal<SelectorType>::value &&
                                                                                                utils::isContainer<ContainerType>::value
                                                                                            >>    
SignalType mux(const SelectorType &selector, const ContainerType &inputs)  {                                 
    HCL_DESIGNCHECK_HINT(!inputs.empty(), "Inputs can not be empty");
    
    const hlim::Node_Signal *firstSignal = inputs.begin()->getNode();
    HCL_ASSERT(firstSignal != nullptr);
    
    hlim::Node_Multiplexer *node = DesignScope::createNode<hlim::Node_Multiplexer>(inputs.size());
    node->recordStackTrace();
    selector.getNode()->getOutput(0).connect(node->getInput(0));
    for (const auto &pair : ConstEnumerate(inputs)) {
        const hlim::Node_Signal *thisSignal = pair.second.getNode();
        HCL_ASSERT(thisSignal != nullptr);
        HCL_DESIGNCHECK_HINT(firstSignal->getConnectionType() == thisSignal->getConnectionType(), "Can only multiplex operands of same type (e.g. width).");

        pair.second.getNode()->getOutput(0).connect(node->getInput(pair.first+1));
    }
    
    return SignalType(&node->getOutput(0), firstSignal->getConnectionType());
}
#endif



///@todo overload for compound signals
template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>    
void driveWith(SignalType &dst, const SignalType &src)  {                                 
    hlim::Node_Signal *signalNode = dst.getNode();
    HCL_ASSERT(signalNode != nullptr);
    
    HCL_DESIGNCHECK_HINT(signalNode->getDriver(0).node == nullptr, "Signal is already being driven.");
    HCL_ASSERT(src.getNode() != nullptr);
    
    signalNode->connectInput({.node = src.getNode(), .port = 0ull});
    signalNode->moveToGroup(GroupScope::getCurrentNodeGroup());
}



//cat(....)



}
