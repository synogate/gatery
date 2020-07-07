#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"
#include "ConditionalScope.h"

#include <hcl/hlim/coreNodes/Node_Multiplexer.h>
#include <hcl/hlim/supportNodes/Node_SignalTap.h>
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
    
    HCL_ASSERT_HINT(ConditionalScope::get() == nullptr, "Using driveWith in conditional scopes (IF ELSE) not yet implemented!");
    
    hlim::Node_Signal *signalNode = dst.getNode();
    HCL_ASSERT(signalNode != nullptr);
    
    HCL_DESIGNCHECK_HINT(signalNode->getDriver(0).node == nullptr, "Signal is already being driven.");
    HCL_ASSERT(src.getNode() != nullptr);
    
    signalNode->connectInput({.node = src.getNode(), .port = 0ull});
    signalNode->moveToGroup(GroupScope::getCurrentNodeGroup());
}



template<typename SignalType, typename = std::enable_if_t<utils::isBitVectorSignal<SignalType>::value>>    
SignalType cat(const std::vector<const ElementarySignal*> signals)  {
    hlim::Node_Rewire *node = DesignScope::createNode<hlim::Node_Rewire>(signals.size());
    node->recordStackTrace();
    
    hlim::Node_Rewire::RewireOperation op;
    op.ranges.resize(signals.size());
    
    for (auto i : utils::Range(signals.size())) {
        node->connectInput(i, {.node = signals[signals.size()-1-i]->getNode(), .port = 0ull});
        op.ranges[i].subwidth = signals[signals.size()-1-i]->getWidth();
        op.ranges[i].source = hlim::Node_Rewire::OutputRange::INPUT;
        op.ranges[i].inputIdx = i;
        op.ranges[i].inputOffset = 0;
    }
    node->setOp(op);
    {
        SignalType dummy(1);
        node->changeOutputType(dummy.getNode()->getOutputConnectionType(0));
    }
    
    return SignalType({.node = node, .port = 0ull});
}


template<class VectorType, class ... Types>
struct GetFirstVectorType {
    using type = std::enable_if_t<utils::isBitVectorSignal<VectorType>::value, VectorType>;
};

template<class ... Types>
struct GetFirstVectorType<Bit, Types...> {
    using type = typename GetFirstVectorType<Types...>::type;
};


template<class Signal>
void collectSignals(std::vector<const ElementarySignal*> &signals, const Signal &signal) {
    signals.push_back(&signal);
}

template<class Signal, class ... Types>
void collectSignals(std::vector<const ElementarySignal*> &signals, const Signal &signal, const Types&... remaining) {
    signals.push_back(&signal);
    collectSignals(signals, remaining...);
}



template<typename... Types>
typename GetFirstVectorType<Types...>::type cat(const Types&... allSignals)
{
    static_assert(sizeof...(Types) > 0, "Can not concatenate empty list of signals!");

    std::vector<const ElementarySignal*> signals;
    collectSignals(signals, allSignals...);
    return cat<typename GetFirstVectorType<Types...>::type>(signals);
}


/*
template<typename SignalType, typename = std::enable_if_t<utils::isBitVectorSignal<SignalType>::value>>    
class hex
{
};

todo: pretty printing functions
*/


class SignalTapHelper
{
    public:
        SignalTapHelper(hlim::Node_SignalTap::Level level);
        
        void triggerIf(const Bit &condition);
        void triggerIfNot(const Bit &condition);
        
        SignalTapHelper &operator<<(const std::string &msg);
        
        template<typename type, typename = std::enable_if_t<std::is_arithmetic<type>::value>>
        SignalTapHelper &operator<<(type number) { return (*this) << boost::lexical_cast<std::string>(number); }

        template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>    
        SignalTapHelper &operator<<(const SignalType &signal) {
            unsigned port = addInput({.node = signal.getNode(), .port = 0ull});
            m_node->addMessagePart(hlim::Node_SignalTap::FormattedSignal{.inputIdx = port, .format = 0});
            return *this;
        }
    protected:
        unsigned addInput(hlim::NodePort nodePort);
        hlim::Node_SignalTap *m_node = nullptr;
};




SignalTapHelper sim_assert(const Bit &condition);
SignalTapHelper sim_warnIf(const Bit &condition);
    
SignalTapHelper sim_debug();
SignalTapHelper sim_debugIf(const Bit &condition);



}
