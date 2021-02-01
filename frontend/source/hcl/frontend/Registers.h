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
        
        // TODO: add default and copy constructor
        ~Register();

        template<typename... Args>
        explicit Register(Args... signalParams);

        Register(const Register<SignalType>& rhs) = delete;

        Register<SignalType>& setEnable(const Bit& enableSignal);
        Register<SignalType>& setReset(const SignalType& resetValue);
        Register<SignalType>& setClock(const Clock& clock);
        
        Register<SignalType>& operator=(const Register<SignalType>& rhs) { SignalType::assign(rhs.getReadPort()); return *this; }
        Register<SignalType>& operator=(const SignalType& rhs) { SignalType::assign(rhs.getReadPort()); return *this; }
        
        const SignalType &delay(size_t ticks = 1);
        void reset();
        
        virtual void setName(std::string name) override;        

    protected:

        hlim::Node_Register& m_regNode;
        SignalType m_delayedSignal;
        std::optional<SignalType> m_resetSignal;
};

template<typename SignalType>
template<typename ...Args>
inline frontend::Register<SignalType>::Register(Args ...signalParams) :
    SignalType{signalParams...},
    m_regNode{*DesignScope::createNode<hlim::Node_Register>()}
{
    HCL_ASSERT(SignalType::valid());

    m_regNode.recordStackTrace();


    setClock(ClockScope::getClk());
    m_regNode.connectInput(hlim::Node_Register::DATA, this->getReadPort());

    m_delayedSignal = SignalType(SignalReadPort(&m_regNode));

    // specialize on undefined value ctors
    if constexpr (sizeof...(Args) == 2 && std::is_same_v<SignalType, BVec>)
    {
        if constexpr (
            std::is_same_v<BitWidth, std::tuple_element_t<0, std::tuple<Args...>>> &&
            std::is_same_v<Expansion, std::tuple_element_t<1, std::tuple<Args...>>>
            )
            SignalType::assign(m_delayedSignal.getReadPort());
    }

    if constexpr (sizeof...(Args) == 1 && std::is_same_v<SignalType, BVec>)
    {
        if constexpr (std::is_same_v<BitWidth, std::tuple_element_t<0, std::tuple<Args...>>>)
            SignalType::assign(m_delayedSignal.getReadPort());
    }

    if constexpr (sizeof...(Args) == 0 && std::is_same_v<SignalType, Bit>)
    {
        SignalType::assign(m_delayedSignal.getReadPort());
    }
}

template<typename SignalType>
inline Register<SignalType>::~Register()
{
   // HCL_ASSERT(m_node.getDriver(hlim::Node_Register::DATA).node == SignalType::getNode());
}

template<typename SignalType>
inline Register<SignalType>& Register<SignalType>::setEnable(const Bit& enableSignal)
{
    m_regNode.connectInput(hlim::Node_Register::ENABLE, enableSignal.getReadPort());
    return *this;
}

template<typename SignalType>
inline Register<SignalType>& Register<SignalType>::setReset(const SignalType& resetValue)
{
    m_resetSignal = resetValue;
    m_regNode.connectInput(hlim::Node_Register::RESET_VALUE, m_resetSignal->getReadPort());
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
    HCL_DESIGNCHECK(m_resetSignal);
    assign(m_resetSignal->getReadPort());
}

template<typename SignalType>
void Register<SignalType>::setName(std::string name)
{
    if(m_resetSignal)
        m_resetSignal->setName(name+"reset");
    m_delayedSignal.setName(name+"delayed_1");
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




