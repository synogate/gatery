#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Multiplexer.h"
#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"


#include <boost/format.hpp>


namespace mhdl {
namespace core {    
namespace frontend {
    
///@todo overload for compound signals
template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>    
SignalType mux(const Bit &selector, const SignalType &lhs, const SignalType &rhs)  {                                 
    const hlim::Node_Signal *lhsSignal = dynamic_cast<const hlim::Node_Signal*>(lhs.getOutputPort()->node);
    const hlim::Node_Signal *rhsSignal = dynamic_cast<const hlim::Node_Signal*>(rhs.getOutputPort()->node);
    MHDL_ASSERT(lhsSignal != nullptr);
    MHDL_ASSERT(rhsSignal != nullptr);
    MHDL_DESIGNCHECK_HINT(lhsSignal->getConnectionType() == rhsSignal->getConnectionType(), "Can only multiplex operands of same type (e.g. width).");
    
    hlim::Node_Multiplexer *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Multiplexer>(2);
    node->recordStackTrace();
    selector.getOutputPort()->connect(node->getInput(0));
    lhs.getOutputPort()->connect(node->getInput(1));
    rhs.getOutputPort()->connect(node->getInput(2));

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
    
    const hlim::Node_Signal *firstSignal = dynamic_cast<const hlim::Node_Signal*>(inputs.begin()->getOutputPort()->node);
    MHDL_ASSERT(firstSignal != nullptr);
    
    hlim::Node_Multiplexer *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Multiplexer>(inputs.size());
    node->recordStackTrace();
    selector.getOutputPort()->connect(node->getInput(0));
    for (const auto &pair : ConstEnumerate(inputs)) {
        const hlim::Node_Signal *thisSignal = dynamic_cast<const hlim::Node_Signal*>(pair.second.getOutputPort()->node);
        MHDL_ASSERT(thisSignal != nullptr);
        MHDL_DESIGNCHECK_HINT(firstSignal->getConnectionType() == thisSignal->getConnectionType(), "Can only multiplex operands of same type (e.g. width).");

        pair.second.getOutputPort()->connect(node->getInput(pair.first+1));
    }
    
    return SignalType(&node->getOutput(0), firstSignal->getConnectionType());
}




///@todo overload for compound signals
template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>    
void driveWith(SignalType &dst, const SignalType &src)  {                                 
    MHDL_ASSERT(dst.getOutputPort() != nullptr);
    hlim::Node_Signal *signalNode = dynamic_cast<hlim::Node_Signal*>(dst.getOutputPort()->node);
    MHDL_ASSERT(signalNode != nullptr);
    
    MHDL_DESIGNCHECK_HINT(signalNode->getInput(0).connection == nullptr, "Signal is already being driven.");
    
    MHDL_ASSERT(src.getOutputPort() != nullptr);
    src.getOutputPort()->connect(signalNode->getInput(0));
}




}
}
}
