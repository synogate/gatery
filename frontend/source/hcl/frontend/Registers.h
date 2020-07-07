#pragma once

#include "SignalDelay.h"
#include "Scope.h"
#include "Bit.h"
#include "SignalMiscOp.h"
#include "Constant.h"

#include <hcl/utils/Traits.h>
#include <hcl/hlim/coreNodes/Node_Register.h>

#include <map>

namespace hcl::core::frontend {

class Bit;    
    
struct RegisterConfig {
    hlim::BaseClock *clk = nullptr;
    // bool triggerRisingEdge = true;
    // bool asyncReset = true;
    std::string resetName = "reset";
};

class RegisterFactory
{
    public:
        RegisterFactory(const RegisterConfig &registerConfig);
        
        ///@todo overload for compound signals
        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal operator()(const DataSignal &inputSignal, const Bit &enableSignal, const DataSignal &resetValue);
    protected:
        RegisterConfig m_registerConfig; 
};


///@todo overload for compound signals
template<typename SignalType>
class Register : public SignalType
{
    public:
        using SigType = std::enable_if_t<utils::isElementarySignal<SignalType>::value, SignalType>;
        
        Register(const RegisterConfig &registerConfig, SignalType resetSignal);
        ~Register();
        inline Register<SignalType> &setEnable(Bit enableSignal) { m_enableSignal = enableSignal; return *this; }
        
        Register<SignalType> &operator=(const SignalType &signal);
        //operator SignalType() const { return m_newSignal; }
        
        const SignalType &delay(size_t ticks);
        
        virtual void setName(std::string name) override;        
    protected:
        RegisterConfig m_registerConfig;
        Bit m_enableSignal;
        SignalType m_resetSignal;
        SignalType m_delayedSignal;
};

template<>
Register<Bit>::Register(const RegisterConfig &registerConfig, Bit resetSignal);

template<typename SignalType>
Register<SignalType>::Register(const RegisterConfig &registerConfig, SignalType resetSignal)
{
    m_registerConfig = registerConfig;
    m_enableSignal = 1_bit;
    m_resetSignal = resetSignal;
    m_delayedSignal.resize(m_resetSignal.getWidth());
}


template<typename SignalType>
Register<SignalType>::~Register()
{
    hlim::Node_Register *node = DesignScope::createNode<hlim::Node_Register>();
    node->recordStackTrace();
    //node->connectInput(hlim::Node_Register::DATA, {.node = m_newSignal.getNode(), .port = 0ull});
    node->connectInput(hlim::Node_Register::DATA, {.node = SignalType::getNode(), .port = 0ull});
    node->connectInput(hlim::Node_Register::RESET_VALUE, {.node = m_resetSignal.getNode(), .port = 0ull});
    node->connectInput(hlim::Node_Register::ENABLE, {.node = m_enableSignal.getNode(), .port = 0ull});
    
    node->setClock(m_registerConfig.clk);
    node->setReset(m_registerConfig.resetName);
    
    driveWith(m_delayedSignal, SignalType({.node = node, .port = 0ull}));
}

template<typename SignalType>
Register<SignalType> &Register<SignalType>::operator=(const SignalType &signal)
{ 
    HCL_DESIGNCHECK_HINT(signal.getWidth() == m_resetSignal.getWidth(), "Input signals to a register must match it's reset signal in width"); 
    //m_newSignal = signal;
    SignalType::operator=(signal);
    return *this;
}

template<typename SignalType>
const SignalType &Register<SignalType>::delay(size_t ticks)
{
    HCL_ASSERT_HINT(ticks == 1, "Only delays of one tick are implemented so far!");
    return m_delayedSignal;
}

template<typename SignalType>
void Register<SignalType>::setName(std::string name)
{
    m_resetSignal.setName(name+"_reset");
    m_enableSignal.setName(name+"_enable");
    m_delayedSignal.setName(name+"_delayed_1");
    SignalType::setName(std::move(name));
}




class PipelineRegisterFactory : public RegisterFactory
{
    public:
        PipelineRegisterFactory(const RegisterConfig &registerConfig);
        
        ///@todo overload for compound signals
        template<typename DataSignal, typename = std::enable_if_t<utils::isElementarySignal<DataSignal>::value>>
        DataSignal delayBy(DataSignal inputSignal, Bit enableSignal, DataSignal resetValue, size_t ticks);

        ///@todo overload for compound signals
        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal delayBy(DataSignal inputSignal, Bit enableSignal, DataSignal resetValue, SignalDelay delay);
    protected:
        RegisterConfig m_registerConfig; 
};




template<typename DataSignal, typename>
DataSignal RegisterFactory::operator()(const DataSignal &inputSignal, const Bit &enableSignal, const DataSignal &resetValue)
{
    HCL_DESIGNCHECK_HINT(inputSignal.getNode()->getOutputConnectionType(0) == resetValue.getNode()->getOutputConnectionType(0), "The connection types of the input and reset signals must be the same!");
    
    hlim::Node_Register *node = DesignScope::createNode<hlim::Node_Register>();
    node->recordStackTrace();
    node->connectInput(hlim::Node_Register::DATA, {.node = inputSignal.getNode(), .port = 0ull});
    node->connectInput(hlim::Node_Register::RESET_VALUE, {.node = resetValue.getNode(), .port = 0ull});
    node->connectInput(hlim::Node_Register::ENABLE, {.node = enableSignal.getNode(), .port = 0ull});
    
    node->setClock(m_registerConfig.clk);
    node->setReset(m_registerConfig.resetName);
    
    return DataSignal({.node = node, .port = 0ull});
}


}
