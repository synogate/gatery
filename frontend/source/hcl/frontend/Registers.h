#pragma once

#include "SignalDelay.h"
#include "Scope.h"
#include "Clock.h"
#include "Bit.h"
#include "SignalMiscOp.h"
#include "Constant.h"

#include <hcl/utils/Traits.h>
#include <hcl/hlim/coreNodes/Node_Register.h>

#include <map>
#include <optional>

namespace hcl::core::frontend {

class Bit;

///@todo overload for compound signals
template<typename SignalType>
class Register : public SignalType
{
    public:
        using SigType = std::enable_if_t<utils::isElementarySignal<SignalType>::value, SignalType>;
        using PortType = typename SignalType::PortType;
        
        // TODO: add default and copy constructor
        ~Register();

        template<typename... Args>
        explicit Register(Args... signalParams);

        Register(const Register<SignalType>& rhs) = delete;

        Register<SignalType>& setEnable(BitSignalPort enableSignal);
        Register<SignalType>& setReset(PortType resetValue);
        Register<SignalType>& setClock(const Clock& clock);
        
        Register<SignalType>& operator=(const Register<SignalType>& rhs) { assign(PortType{ rhs }); return *this; }
        Register<SignalType>& operator=(PortType signal) { assign(signal); return *this; }
        
        const SignalType &delay(size_t ticks = 1);
        void reset();
        
        virtual void setName(std::string name) override;        

    protected:
        void assign(const SignalPort& value) override;

        hlim::Node_Register& m_regNode;
        SignalType m_delayedSignal;
        std::optional<PortType> m_resetSignal;
};

template<typename SignalType>
template<typename ...Args>
inline frontend::Register<SignalType>::Register(Args ...signalParams) :
    SignalType{signalParams...},
    m_regNode{*DesignScope::createNode<hlim::Node_Register>()}
{
    m_regNode.recordStackTrace();
    m_regNode.connectInput(hlim::Node_Register::DATA, {.node = SignalType::m_node, .port = 0ull});
    // TODO: connect Enable to global ConditionalScope

    setClock(ClockScope::getClk());

    m_delayedSignal = SignalType({ .node = &m_regNode, .port = 0ull });

    // default assign register output to self
    if (!SignalType::m_node->getDriver(0).node)
        assign(typename SignalType::PortType{ m_delayedSignal });
}

template<typename SignalType>
inline Register<SignalType>::~Register()
{
   // HCL_ASSERT(m_node.getDriver(hlim::Node_Register::DATA).node == SignalType::getNode());
}

template<typename SignalType>
inline Register<SignalType>& Register<SignalType>::setEnable(BitSignalPort enableSignal)
{
    m_regNode.connectInput(hlim::Node_Register::ENABLE, enableSignal.getReadPort());
    return *this;
}

template<typename SignalType>
inline Register<SignalType>& Register<SignalType>::setReset(PortType resetValue)
{
    m_regNode.connectInput(hlim::Node_Register::RESET_VALUE, resetValue.getReadPort());
    m_resetSignal = resetValue;
    return *this;
}

template<typename SignalType>
inline Register<SignalType>& Register<SignalType>::setClock(const Clock& clock)
{
    m_regNode.setClock(clock.getClk());
    return *this;
}

template<typename SignalType>
const SignalType &Register<SignalType>::delay(size_t ticks)
{
    HCL_ASSERT_HINT(ticks == 1, "Only delays of one tick are implemented so far!");
    return m_delayedSignal;
}

template<typename SignalType>
inline void Register<SignalType>::reset()
{
    HCL_ASSERT(m_resetSignal);
    assign(*m_resetSignal);
}

template<typename SignalType>
inline void Register<SignalType>::assign(const SignalPort& value)
{
    HCL_DESIGNCHECK_HINT(value.getWidth() == SignalType::getWidth(), "Input signals to a register must match it's signal in width");
    SignalType::assign(value);
    m_regNode.connectInput(hlim::Node_Register::DATA, SignalType::getReadPort());
}

template<typename SignalType>
void Register<SignalType>::setName(std::string name)
{
    if(m_resetSignal)
        m_resetSignal->setName(name+"_reset");
    m_delayedSignal.setName(name+"_delayed_1");
    SignalType::setName(std::move(name));
}


/*
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
*/

}




