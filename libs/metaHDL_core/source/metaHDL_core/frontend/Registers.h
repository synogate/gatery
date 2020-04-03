#pragma once

#include "SignalDelay.h"

namespace mhdl {
namespace core {    
namespace frontend {

class ClockConfig {
};

class ResetConfig {
};

class RegisterFactory
{
    public:
        RegisterFactory(const ClockConfig &clockConfig, const ResetConfig &resetConfig);
        
        template<typename DataSignal, typename EnableSignal>
        DataSignal reg(DataSignal inputSignal, EnableSignal enableSignal, DataSignal resetValue, EnableSignal resetSignal);
    protected:
};

class PipelineRegisterFactory : public RegisterFactory
{
    public:
        PipelineRegisterFactory(const ClockConfig &clockConfig, const ResetConfig &resetConfig);
        
        template<typename DataSignal, typename EnableSignal>
        DataSignal delayBy(DataSignal inputSignal, EnableSignal enableSignal, DataSignal resetValue, unsigned ticks);

        template<typename DataSignal, typename EnableSignal>
        DataSignal delayBy(DataSignal inputSignal, EnableSignal enableSignal, DataSignal resetValue, SignalDelay delay);
    protected:
};


}
}
}
