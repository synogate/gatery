#pragma once

#include "SignalDelay.h"
#include "../utils/Traits.h"

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
        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal delayBy(DataSignal inputSignal, Bit enableSignal, DataSignal resetValue, unsigned ticks);

        ///@todo overload for compound signals
        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal delayBy(DataSignal inputSignal, Bit enableSignal, DataSignal resetValue, SignalDelay delay);
    protected:
};




template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
DataSignal RegisterFactory::operator()(const DataSignal &inputSignal, const Bit &enableSignal, const DataSignal &resetValue)
{
    return inputSignal; /// @todo
}


}
}
}
