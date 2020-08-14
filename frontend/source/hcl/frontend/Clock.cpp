#include "Clock.h"
#include "Bit.h"

#include <hcl/hlim/coreNodes/Node_Clk2Signal.h>

namespace hcl::core::frontend {
    

Clock::Clock(const ClockConfig &config)
{
    HCL_DESIGNCHECK_HINT(config.m_absoluteFrequency, "A root clock must have an absolute frequency defined through it's ClockConfig!");
    HCL_DESIGNCHECK_HINT(!config.m_frequencyMultiplier, "A root clock mustmust not have a parent relative frequency multiplier defined through it's ClockConfig!");
    m_clock = DesignScope::createClock<hlim::RootClock>(config.m_name?*config.m_name:std::string("sysclk"), *config.m_absoluteFrequency);
    applyConfig(config);
}

Clock::Clock(const Clock &other)
{
    operator=(other);
}

Clock &Clock::operator=(const Clock &other)
{
    if (other.m_clock->getParentClock() == nullptr) {
        m_clock = DesignScope::createClock<hlim::RootClock>(m_clock->getName(), m_clock->getAbsoluteFrequency());
        ///@todo copy attributes
    } else {
        m_clock = DesignScope::createClock<hlim::DerivedClock>(other.m_clock->getParentClock());
        ///@todo copy attributes
    }
    return *this;
}


Clock::Clock(hlim::Clock *clock, const ClockConfig &config) : m_clock(clock)
{
    if (config.m_absoluteFrequency) {
        HCL_ASSERT_HINT(false, "Absolute frequencies on derived clocks not implemented yet!");
    }
    if (config.m_frequencyMultiplier) dynamic_cast<hlim::DerivedClock*>(m_clock)->setFrequencyMuliplier(*config.m_frequencyMultiplier);
    applyConfig(config);
    
}

void Clock::applyConfig(const ClockConfig &config)
{
    if (config.m_name) m_clock->setName(*config.m_name);
    if (config.m_resetName) m_clock->setResetName(*config.m_resetName);
    if (config.m_triggerEvent) m_clock->setTriggerEvent(*config.m_triggerEvent);
    if (config.m_resetType) m_clock->setResetType(*config.m_resetType);
    if (config.m_initializeRegs) m_clock->setInitializeRegs(*config.m_initializeRegs);
    if (config.m_resetHighActive) m_clock->setResetHighActive(*config.m_resetHighActive);
    if (config.m_phaseSynchronousWithParent) m_clock->setPhaseSynchronousWithParent(*config.m_phaseSynchronousWithParent);
    
    HCL_DESIGNCHECK_HINT(m_clock->getResetType() != ResetType::NONE || m_clock->getInitializeRegs(), "Either a type of reset, or the initialization for registers should be enabled!");
}



Clock Clock::deriveClock(const ClockConfig &config)
{
    return Clock(DesignScope::createClock<hlim::DerivedClock>(m_clock), config);
}

Bit Clock::driveSignal()
{
    hlim::Node_Clk2Signal *node = DesignScope::createNode<hlim::Node_Clk2Signal>();
    node->recordStackTrace();

    node->setClock(m_clock);
 
    return SignalReadPort(node);
}


}
