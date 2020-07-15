#pragma once

#include "SignalDelay.h"
#include "Scope.h"
#include "Bit.h"
#include "SignalMiscOp.h"
#include "Constant.h"

#include <hcl/utils/Traits.h>
#include <hcl/hlim/coreNodes/Node_Register.h>

#include <map>
#include <optional>

namespace hcl::core::frontend {

class Bit;    
    
struct RegisterConfig {
    hlim::Clock *clk = nullptr;
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

        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal operator()(const DataSignal& inputSignal, const Bit& enableSignal);

        template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
        DataSignal operator()(const DataSignal& inputSignal);

        const RegisterConfig& config() const { return m_registerConfig; }

protected:
        RegisterConfig m_registerConfig; 
};


///@todo overload for compound signals
template<typename SignalType>
class Register : public SignalType
{
    public:
        using SigType = std::enable_if_t<utils::isElementarySignal<SignalType>::value, SignalType>;
        
        // TODO: add default and copy constructor
        ~Register();

        template<typename... Args>
        Register(const RegisterConfig& registerConfig, Args... signalParams);

        template<typename ...Args>
        Register(const SignalType& resetSignal, const RegisterConfig& registerConfig, Args... signalParams);
        Register(const SignalType& resetSignal, const RegisterConfig& registerConfig);

        Register(const Register<SignalType>& rhs) = delete;

        Register<SignalType>& setEnable(const Bit& enableSignal);
        Register<SignalType>& setReset(const SignalType& resetValue);
        Register<SignalType>& setClock(const RegisterConfig& registerConfig);
        
        Register<SignalType>& operator=(const Register<SignalType>& rhs) { assign(rhs); return *this; }
        Register<SignalType> &operator=(const SignalType &signal) { assign(signal); return *this; }
        
        const SignalType &delay(size_t ticks = 1);
        void reset();
        
        virtual void setName(std::string name) override;        
    protected:
        void assign(const ElementarySignal& value) override;

        hlim::Node_Register& m_node;
        SignalType m_delayedSignal;
        std::optional<SignalType> m_resetSignal;
};

template<typename SignalType>
template<typename ...Args>
inline frontend::Register<SignalType>::Register(const RegisterConfig& registerConfig, Args ...signalParams) :
    SignalType{signalParams...},
    m_node{*DesignScope::createNode<hlim::Node_Register>()}
{
    const core::hlim::NodePort signalPort{ .node = SignalType::getNode(), .port = 0ull };

    m_node.recordStackTrace();
    m_node.connectInput(hlim::Node_Register::DATA, signalPort);
    // TODO: connect Enable to global ConditionalScope

    setEnable(true);
    setClock(registerConfig);

    m_delayedSignal = SignalType({ .node = &m_node, .port = 0ull });

    // default assign register output to self
    if (!signalPort.node->getDriver(0).node)
        assign(m_delayedSignal);

}

template<typename SignalType>
template<typename ...Args>
Register<SignalType>::Register(const SignalType& resetSignal, const RegisterConfig &registerConfig, Args... signalParams) :
    Register<SignalType>{registerConfig, signalParams...}
{
    setReset(resetSignal);
}

template<typename SignalType>
inline Register<SignalType>::~Register()
{
    HCL_ASSERT(m_node.getDriver(hlim::Node_Register::DATA).node == SignalType::getNode());
}

template<typename SignalType>
inline Register<SignalType>::Register(const SignalType& resetSignal, const RegisterConfig& registerConfig) :
    Register{ resetSignal, registerConfig, resetSignal.getWidth() }
{
}

template<>
inline Register<Bit>::Register(const Bit& resetSignal, const RegisterConfig& registerConfig) :
    Register{ resetSignal, registerConfig, Bit{} }
{
}

template<typename SignalType>
inline Register<SignalType>& Register<SignalType>::setEnable(const Bit& enableSignal)
{
    m_node.connectInput(hlim::Node_Register::ENABLE, { .node = enableSignal.getNode(), .port = 0ull });
    return *this;
}

template<typename SignalType>
inline Register<SignalType>& Register<SignalType>::setReset(const SignalType& resetValue)
{
    m_node.connectInput(hlim::Node_Register::RESET_VALUE, { .node = resetValue.getNode(), .port = 0ull });
    m_resetSignal = resetValue;
    return *this;
}

template<typename SignalType>
inline Register<SignalType>& Register<SignalType>::setClock(const RegisterConfig& registerConfig)
{
    m_node.setClock(registerConfig.clk);
    m_node.setReset(registerConfig.resetName);
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
inline void Register<SignalType>::assign(const ElementarySignal& value)
{
    HCL_DESIGNCHECK_HINT(value.getWidth() == SignalType::getWidth(), "Input signals to a register must match it's signal in width");
    SignalType::assign(value);
    m_node.connectInput(hlim::Node_Register::DATA, { .node = SignalType::getNode(), .port = 0ull });
}

template<typename SignalType>
void Register<SignalType>::setName(std::string name)
{
    if(m_resetSignal)
        m_resetSignal->setName(name+"_reset");
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

template<typename DataSignal, typename>
DataSignal RegisterFactory::operator()(const DataSignal& inputSignal, const Bit& enableSignal)
{
    hlim::Node_Register* node = DesignScope::createNode<hlim::Node_Register>();
    node->recordStackTrace();
    node->connectInput(hlim::Node_Register::DATA, { .node = inputSignal.getNode(), .port = 0ull });
    node->connectInput(hlim::Node_Register::ENABLE, { .node = enableSignal.getNode(), .port = 0ull });

    node->setClock(m_registerConfig.clk);
    node->setReset(m_registerConfig.resetName);

    return DataSignal({ .node = node, .port = 0ull });
}

template<typename DataSignal, typename>
DataSignal RegisterFactory::operator()(const DataSignal& inputSignal)
{
    hlim::Node_Register* node = DesignScope::createNode<hlim::Node_Register>();
    node->recordStackTrace();
    node->connectInput(hlim::Node_Register::DATA, { .node = inputSignal.getNode(), .port = 0ull });

    node->setClock(m_registerConfig.clk);
    node->setReset(m_registerConfig.resetName);

    return DataSignal({ .node = node, .port = 0ull });
}

}




