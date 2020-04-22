#pragma once

#include "SignalDelay.h"
#include "Scope.h"
#include "../utils/Traits.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../frontend/Bit.h"

namespace mhdl::core::frontend {

class Bit;    
    
class ClockConfig {
};

class ResetConfig {
};

class RegisterFactory
{
    public:
        RegisterFactory(const ClockConfig &clockConfig, const ResetConfig &resetConfig);
        
        ///@todo overload for compound signals
        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal operator()(const DataSignal &inputSignal, const Bit &enableSignal, const DataSignal &resetValue);
    protected:
};

class PipelineRegisterFactory : public RegisterFactory
{
    public:
        PipelineRegisterFactory(const ClockConfig &clockConfig, const ResetConfig &resetConfig);
        
        ///@todo overload for compound signals
        template<typename DataSignal, typename = std::enable_if_t<utils::isElementarySignal<DataSignal>::value>>
        DataSignal delayBy(DataSignal inputSignal, Bit enableSignal, DataSignal resetValue, size_t ticks);

        ///@todo overload for compound signals
        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal delayBy(DataSignal inputSignal, Bit enableSignal, DataSignal resetValue, SignalDelay delay);
    protected:
};




template<typename DataSignal, typename>
DataSignal RegisterFactory::operator()(const DataSignal &inputSignal, const Bit &enableSignal, const DataSignal &resetValue)
{
    MHDL_DESIGNCHECK_HINT(inputSignal.getNode()->getOutputConnectionType(0) == resetValue.getNode()->getOutputConnectionType(0), "The connection types of the input and reset signals must be the same!");
    
    hlim::Node_Register *node = DesignScope::createNode<hlim::Node_Register>();
    node->recordStackTrace();
    node->connectInput(hlim::Node_Register::DATA, {.node = inputSignal.getNode(), .port = 0ull});
    node->connectInput(hlim::Node_Register::RESET_VALUE, {.node = resetValue.getNode(), .port = 0ull});
    node->connectInput(hlim::Node_Register::ENABLE, {.node = enableSignal.getNode(), .port = 0ull});
    
    return DataSignal({.node = node, .port = 0ull});
}


}
