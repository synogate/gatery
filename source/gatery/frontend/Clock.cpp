/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "gatery/pch.h"
#include "Clock.h"
#include "Bit.h"
#include "ConditionalScope.h"

#include <gatery/hlim/coreNodes/Node_Clk2Signal.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

namespace gtry {


Clock::Clock(size_t freq) : Clock(ClockConfig{}.setAbsoluteFrequency({freq, 1}))
{
}

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
    //m_clock = DesignScope::get()->getCircuit().createUnconnectedClock(other.m_clock, other.m_clock->getParentClock());
    m_clock = other.m_clock;
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
    if (config.m_phaseSynchronousWithParent) m_clock->setPhaseSynchronousWithParent(*config.m_phaseSynchronousWithParent);

    // By default, use the same behavior for memory and registers ...
    if (config.m_resetType) m_clock->getRegAttribs().memoryResetType = m_clock->getRegAttribs().resetType = *config.m_resetType;
    if (config.m_initializeRegs) m_clock->getRegAttribs().initializeMemory = m_clock->getRegAttribs().initializeRegs = *config.m_initializeRegs;

    // ... unless overruled explicitely
    if (config.m_memoryResetType) m_clock->getRegAttribs().memoryResetType = *config.m_memoryResetType;
    if (config.m_initializeMemory) m_clock->getRegAttribs().initializeMemory = *config.m_initializeMemory;

    if (config.m_resetHighActive) m_clock->getRegAttribs().resetHighActive = *config.m_resetHighActive;

    if (config.m_registerEnablePinUsage) m_clock->getRegAttribs().registerEnablePinUsage = *config.m_registerEnablePinUsage;
    if (config.m_registerResetPinUsage) m_clock->getRegAttribs().registerResetPinUsage = *config.m_registerResetPinUsage;

    HCL_DESIGNCHECK_HINT(m_clock->getRegAttribs().resetType != ResetType::NONE || m_clock->getRegAttribs().initializeRegs, "Either a type of reset, or the initialization for registers should be enabled!");

    for (const auto &vendor : config.m_additionalUserDefinedVendorAttributes)
        for (const auto &attrib : vendor.second)
            m_clock->getRegAttribs().userDefinedVendorAttributes[vendor.first][attrib.first] = attrib.second;
}



Clock Clock::deriveClock(const ClockConfig &config)
{
    return Clock(DesignScope::createClock<hlim::DerivedClock>(m_clock), config);
}

/*
Bit Clock::driveSignal()
{
    hlim::Node_Clk2Signal *node = DesignScope::createNode<hlim::Node_Clk2Signal>();
    node->recordStackTrace();

    node->setClock(m_clock);

    return SignalReadPort(node);
}
*/

BVec Clock::operator()(const BVec& signal, const RegisterSettings &settings) const
{
    SignalReadPort data = signal.getReadPort();

    auto* reg = DesignScope::createNode<hlim::Node_Register>();
    reg->setName(std::string{ signal.getName() });
    reg->setClock(m_clock);
    reg->connectInput(hlim::Node_Register::DATA, data);

    if (settings.allowRetimingForward)
        reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD);

    if (settings.allowRetimingBackward)
        reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD);

    ConditionalScope* scope = ConditionalScope::get();
    if (scope)
    {
        reg->connectInput(hlim::Node_Register::ENABLE, scope->getFullCondition());
        reg->setConditionId(scope->getId());
    }

    return SignalReadPort(reg, data.expansionPolicy);
}

BVec Clock::operator()(const BVec& signal, const BVec& reset, const RegisterSettings &settings) const
{
    NormalizedWidthOperands ops(signal, reset);

    auto* reg = DesignScope::createNode<hlim::Node_Register>();
    reg->setName(std::string{ signal.getName() });
    reg->setClock(m_clock);
    reg->connectInput(hlim::Node_Register::DATA, ops.lhs);
    reg->connectInput(hlim::Node_Register::RESET_VALUE, ops.rhs);

    if (settings.allowRetimingForward)
        reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD);

    if (settings.allowRetimingBackward)
        reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD);


    ConditionalScope* scope = ConditionalScope::get();
    if (scope)
    {
        reg->connectInput(hlim::Node_Register::ENABLE, scope->getFullCondition());
        reg->setConditionId(scope->getId());
    }

    return SignalReadPort(reg, ops.lhs.expansionPolicy);
}

Bit Clock::operator()(const Bit& signal, const RegisterSettings &settings) const
{
    auto* reg = DesignScope::createNode<hlim::Node_Register>();
    reg->setName(std::string{ signal.getName() });
    reg->setClock(m_clock);
    reg->connectInput(hlim::Node_Register::DATA, signal.getReadPort());

    if (settings.allowRetimingForward)
        reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD);

    if (settings.allowRetimingBackward)
        reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD);

    if(signal.getResetValue())
        reg->connectInput(hlim::Node_Register::RESET_VALUE, Bit{ *signal.getResetValue() }.getReadPort());

    ConditionalScope* scope = ConditionalScope::get();
    if (scope)
    {
        reg->connectInput(hlim::Node_Register::ENABLE, scope->getFullCondition());
        reg->setConditionId(scope->getId());
    }

    Bit ret{ SignalReadPort{reg} };
    if (signal.getResetValue())
        ret.setResetValue(*signal.getResetValue());
    return ret;
}

Bit Clock::operator()(const Bit& signal, const Bit& reset, const RegisterSettings &settings) const
{
    auto* reg = DesignScope::createNode<hlim::Node_Register>();
    reg->setName(std::string{ signal.getName() });
    reg->setClock(m_clock);
    reg->connectInput(hlim::Node_Register::DATA, signal.getReadPort());
    reg->connectInput(hlim::Node_Register::RESET_VALUE, reset.getReadPort());

    if (settings.allowRetimingForward)
        reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD);

    if (settings.allowRetimingBackward)
        reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD);


    ConditionalScope* scope = ConditionalScope::get();
    if (scope)
    {
        reg->connectInput(hlim::Node_Register::ENABLE, scope->getFullCondition());
        reg->setConditionId(scope->getId());
    }

    return SignalReadPort(reg);
}


}
