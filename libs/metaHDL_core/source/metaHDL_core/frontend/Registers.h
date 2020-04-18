#pragma once

#include "SignalDelay.h"
#include "Scope.h"
#include "../utils/Traits.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../frontend/Bit.h"

namespace mhdl {
namespace core {    
namespace frontend {

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
        DataSignal delayBy(DataSignal inputSignal, Bit enableSignal, DataSignal resetValue, unsigned ticks);

        ///@todo overload for compound signals
        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal delayBy(DataSignal inputSignal, Bit enableSignal, DataSignal resetValue, SignalDelay delay);
    protected:
};




template<typename DataSignal, typename>
DataSignal RegisterFactory::operator()(const DataSignal &inputSignal, const Bit &enableSignal, const DataSignal &resetValue)
{
    hlim::Node_Register *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Register>();
    node->recordStackTrace();
    inputSignal.getNode()->getOutput(0).connect(node->getInput(0));
    enableSignal.getNode()->getOutput(0).connect(node->getInput(1));
    resetValue.getNode()->getOutput(0).connect(node->getInput(2));

    return DataSignal(&node->getOutput(0), inputSignal.getNode()->getConnectionType());
}


}
}
}
