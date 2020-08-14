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
template<typename ContainerType>//, typename = std::enable_if_t<utils::isContainer<ContainerType>::value>>    
typename ContainerType::value_type mux(const ElementarySignal &selector, const ContainerType &table)  {      
    hlim::Node_Multiplexer *node = DesignScope::createNode<hlim::Node_Multiplexer>(table.size());
    node->recordStackTrace();
    node->connectSelector(selector.getReadPort());

    HCL_DESIGNCHECK_HINT(table.size() <= (1ull << selector.getWidth()), "The number of mux inputs is larger than can be addressed with it's selector input's width!");
    
    const auto &firstSignal = *begin(table);
    
    size_t idx = 0;
    for (const auto &signal : table) {
        HCL_DESIGNCHECK_HINT(signal.getConnType() == firstSignal.getConnType(), "Can only multiplex operands of same type (e.g. width).");
        node->connectInput(idx, signal.getReadPort());
        idx++;
    }
    
    //using SignalType = typename ContainerType::value_type;

    return SignalReadPort(node);
}

template<typename ElemType>
ElemType mux(const ElementarySignal &selector, const std::initializer_list<ElemType> &table) {
    return mux<std::initializer_list<ElemType>>(selector, table);
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

template<typename... Types>
BVec cat(const Types&... allSignals)
{
    auto check_parameter = [](auto signal) {
        static_assert(std::is_convertible_v<decltype(signal), BVec> | std::is_convertible_v<decltype(signal), Bit>,
            "argument passed to cat is not a signal or constant");
    };
    (check_parameter(allSignals), ...);

    struct
    {
        hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(sizeof...(allSignals));
        size_t offset = sizeof...(allSignals) - 1;

        void operator () (const Bit& signal) { node->connectInput(offset--, signal.getReadPort()); }
        void operator () (const BVec& signal) { node->connectInput(offset--, signal.getReadPort()); }
    } assigner;

    (assigner(allSignals), ...);
    assigner.node->setConcat();
    assigner.node->changeOutputType({ .interpretation = hlim::ConnectionType::BITVEC });

    return SignalReadPort(assigner.node);
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
            unsigned port = addInput(signal.getReadPort());
            m_node->addMessagePart(hlim::Node_SignalTap::FormattedSignal{.inputIdx = port, .format = 0});
            return *this;
        }
    protected:
        size_t addInput(hlim::NodePort nodePort);
        hlim::Node_SignalTap *m_node = nullptr;
};




SignalTapHelper sim_assert(const Bit &condition);
SignalTapHelper sim_warnIf(const Bit &condition);
    
SignalTapHelper sim_debug();
SignalTapHelper sim_debugAlways();
SignalTapHelper sim_debugIf(const Bit &condition);



}
