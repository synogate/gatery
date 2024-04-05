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

#include "DesignScope.h"
#include "EnableScope.h"
#include "trace.h"
#include "SignalLogicOp.h"

#include <gatery/hlim/coreNodes/Node_Clk2Signal.h>
#include <gatery/hlim/coreNodes/Node_ClkRst2Signal.h>
#include <gatery/hlim/coreNodes/Node_Signal2Rst.h>
#include <gatery/hlim/coreNodes/Node_Signal2Clk.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

#include <external/magic_enum.hpp>

#include <boost/algorithm/string.hpp>

#include <gatery/scl/cdc.h>

namespace gtry
{
	hlim::ClockRational clockFromString(std::string text)
	{
		double number = 0;
		std::string unit;
		std::istringstream{ text } >> number >> unit;

		ClockConfig::ClockRational roundedNumber{ uint64_t(number * 1000), 1000 };
		ClockConfig::ClockRational frequency;

		boost::algorithm::to_lower(unit);
		if (unit == "ps")
			frequency = ClockConfig::ClockRational{ 1'000'000'000'000, 1 } / roundedNumber;
		else if (unit == "ns")
			frequency = ClockConfig::ClockRational{ 1'000'000'000, 1 } / roundedNumber;
		else if (unit == "us")
			frequency = ClockConfig::ClockRational{ 1'000'000, 1 } / roundedNumber;
		else if (unit == "ms")
			frequency = ClockConfig::ClockRational{ 1'000, 1 } / roundedNumber;
		else if (unit == "s")
			frequency = ClockConfig::ClockRational{ 1, 1 } / roundedNumber;
		else if (unit == "hz")
			frequency = ClockConfig::ClockRational{ 1, 1 } * roundedNumber;
		else if (unit == "khz")
			frequency = ClockConfig::ClockRational{ 1'000, 1 } * roundedNumber;
		else if (unit == "mhz")
			frequency = ClockConfig::ClockRational{ 1'000'000, 1 } * roundedNumber;
		else if (unit == "ghz")
			frequency = ClockConfig::ClockRational{ 1'000'000'000, 1 } * roundedNumber;
		else if (unit == "thz")
			frequency = ClockConfig::ClockRational{ 1'000'000'000'000, 1 } * roundedNumber;
		else
			throw std::runtime_error{ "unknown clock period unit '" + unit + "'. must be one of (ps, ns, us, ms, s, Hz, KHz, MHz, GHz, THz)" };

		return frequency;
	}

	std::ostream& operator<<(std::ostream& s, const ClockConfig& cfg)
	{
		cfg.print(s);
		return s;
	}

	void ClockConfig::loadConfig(const utils::ConfigTree& config)
	{
		std::optional<std::string> period;
		if (config.isScalar())
			absoluteFrequency = clockFromString(config.as<std::string>());
		else
		{
			if(config["name"])
				name = config["name"].as<std::string>();

			if (config["period"])
				absoluteFrequency = clockFromString(config["period"].as<std::string>());

			if (config["clock_edge"])
				triggerEvent = config["clock_edge"].as<TriggerEvent>();

			if (config["reset_name"])
				resetName = config["reset_name"].as<std::string>();

			if (config["reset_type"])
				resetType = config["reset_type"].as<ResetType>();

			if (config["memory_reset_type"])
				memoryResetType = config["memory_reset_type"].as<ResetType>();

			if (config["reset_active"])
				resetActive = config["reset_active"].as<ResetActive>();

			if (config["initialize_registers"])
				initializeRegs = config["initialize_registers"].as<bool>();
		
			if (config["initialize_memories"])
				initializeMemory = config["initialize_memories"].as<bool>();

			attributes.loadConfig(config["attributes"]);
		}
	}

	void ClockConfig::print(std::ostream& s) const
	{
		auto print = [&](std::string_view label, const auto& value)
		{
			if (value)
			{
				if constexpr (std::is_enum_v<std::remove_reference_t<decltype(*value)>>)
					s << ", " << label << ": " << magic_enum::enum_name(*value);
				else
					s << ", " << label << ": " << *value;
			}
		};

		if (name)
			s << *name;
		
		if (absoluteFrequency)
		{
			double period_mhz = double(absoluteFrequency->numerator()) / absoluteFrequency->denominator();
			period_mhz /= 1'000'000;
			std::string text = std::to_string(size_t(std::round(period_mhz))) + " MHz";
			print("period", std::make_optional(text));
		}

		print("clock edge", triggerEvent);
		print("reset name", resetName);
		print("reset type", resetType);
		print("memory reset type", memoryResetType);
		print("reset active", resetActive);
		print("initialize registers", initializeRegs);
		print("initialize memory", initializeMemory);

		auto it = attributes.userDefinedVendorAttributes.find("all");
		if(it != attributes.userDefinedVendorAttributes.end())
			for (const auto& attr : it->second)
				s << ", " << attr.first << ": " << attr.second.value;

	}
	
	ClockConfig &ClockConfig::addRegisterAttribute(const std::string &vendor, const std::string &attrib, const std::string &type, const std::string &value)
	{ 
		attributes.userDefinedVendorAttributes[vendor][attrib] = {.type = type, .value = value};
		return *this; 
	}

	//Clock::Clock(size_t freq) : Clock(ClockConfig{ .absoluteFrequency = ClockConfig::ClockRational{freq, 1} })
	//{
	//}

	Clock::Clock(const ClockConfig& config)
	{
		HCL_DESIGNCHECK_HINT(config.absoluteFrequency, "A root clock must have an absolute frequency defined through it's ClockConfig!");
		HCL_DESIGNCHECK_HINT(!config.frequencyMultiplier, "A root clock mustmust not have a parent relative frequency multiplier defined through it's ClockConfig!");
		m_clock = DesignScope::createClock<hlim::RootClock>(config.name ? *config.name : std::string("sysclk"), *config.absoluteFrequency);
		applyConfig(config);
	}

	Clock::Clock(const Clock& other)
	{
		operator=(other);
	}

	Clock& Clock::operator=(const Clock& other)
	{
		//m_clock = DesignScope::get()->getCircuit().createUnconnectedClock(other.m_clock, other.m_clock->getParentClock());
		m_clock = other.m_clock;
		return *this;
	}


	Clock::Clock(hlim::Clock* clock, const ClockConfig& config) : m_clock(clock)
	{
		if (config.absoluteFrequency) {
			HCL_ASSERT_HINT(false, "Absolute frequencies on derived clocks not implemented yet!");
		}
		if (config.frequencyMultiplier) dynamic_cast<hlim::DerivedClock*>(m_clock)->setFrequencyMuliplier(*config.frequencyMultiplier);
		applyConfig(config);

	}

	void Clock::applyConfig(const ClockConfig& config)
	{
		if (config.name) m_clock->setName(*config.name);
		if (config.resetName) m_clock->setResetName(*config.resetName);
		if (config.triggerEvent) m_clock->setTriggerEvent(*config.triggerEvent);
		if (config.phaseSynchronousWithParent) m_clock->setPhaseSynchronousWithParent(*config.phaseSynchronousWithParent);

		// By default, use the same behavior for memory and registers ...
		if (config.resetType) m_clock->getRegAttribs().memoryResetType = m_clock->getRegAttribs().resetType = *config.resetType;
		if (config.initializeRegs) m_clock->getRegAttribs().initializeMemory = m_clock->getRegAttribs().initializeRegs = *config.initializeRegs;

		// ... unless overruled explicitely
		if (config.memoryResetType) {
			m_clock->getRegAttribs().memoryResetType = *config.memoryResetType;
			HCL_DESIGNCHECK_HINT(m_clock->getRegAttribs().memoryResetType == ResetType::NONE || m_clock->getRegAttribs().resetType != ResetType::NONE, "If a memory reset type != none is chosen, a regular reset type != none must also be chosen!");
		}
		if (config.initializeMemory) m_clock->getRegAttribs().initializeMemory = *config.initializeMemory;

		if (config.resetActive) m_clock->getRegAttribs().resetActive = *config.resetActive;

		if (config.synchronizationRegister) m_clock->getRegAttribs().synchronizationRegister = *config.synchronizationRegister;
		if (config.registerEnablePinUsage) m_clock->getRegAttribs().registerEnablePinUsage = *config.registerEnablePinUsage;
		if (config.registerResetPinUsage) m_clock->getRegAttribs().registerResetPinUsage = *config.registerResetPinUsage;

		HCL_DESIGNCHECK_HINT(m_clock->getRegAttribs().resetType != ResetType::NONE || m_clock->getRegAttribs().initializeRegs, "Either a type of reset, or the initialization for registers should be enabled!");

		for (const auto& vendor : config.attributes.userDefinedVendorAttributes)
			for (const auto& attrib : vendor.second)
				m_clock->getRegAttribs().userDefinedVendorAttributes[vendor.first][attrib.first] = attrib.second;
	}



	Clock Clock::deriveClock(const ClockConfig& config) const
	{
		return Clock(DesignScope::createClock<hlim::DerivedClock>(m_clock), config);
	}

	Bit Clock::clkSignal() const
	{
		auto *node = DesignScope::createNode<hlim::Node_Clk2Signal>();
		node->setClock(m_clock);
		return SignalReadPort(node);
	}

	Bit Clock::rstSignal() const
	{
		auto *node = DesignScope::createNode<hlim::Node_ClkRst2Signal>();
		node->setClock(m_clock);
		return SignalReadPort(node);
	}

	void Clock::overrideClkWith(const Bit &clkOverride)
	{
		auto *node = DesignScope::createNode<hlim::Node_Signal2Clk>();
		node->connect(clkOverride.readPort());
		m_clock->setLogicClockDriver(node);
	}

	void Clock::overrideRstWith(const Bit &rstOverride)
	{
		auto *node = DesignScope::createNode<hlim::Node_Signal2Rst>();
		node->connect(rstOverride.readPort());
		m_clock->setLogicResetDriver(node);
	}

	Bit Clock::reset(ResetActive active) const
	{
		if (m_clock->getRegAttribs().resetType == ResetType::NONE) {
			if (active == ResetActive::HIGH)
				return Bit('0');
			else
				return Bit('1');
		}

		auto* node = DesignScope::createNode<hlim::Node_ClkRst2Signal>();
		node->setClock(m_clock);
		Bit signal{SignalReadPort(node)};

		if(active != m_clock->getRegAttribs().resetActive)
			signal = !signal;

		return signal;
	}

	void Clock::reset(const Bit& signal)
	{
		Bit dummy;
		dummy.exportOverride(signal);

		auto* node = DesignScope::createNode<hlim::Node_Signal2Rst>();
		node->connect(dummy.readPort());
		m_clock->setLogicResetDriver(node);
	}

	void Clock::reset(const Bit& signal, const Clock& sourceClock)
	{
		Bit synchronizedSignal;
		if(m_clock->getRegAttribs().resetType == ResetType::SYNCHRONOUS)
			synchronizedSignal = scl::synchronize(signal, sourceClock, *this);
		else
			synchronizedSignal = scl::synchronizeRelease(signal, sourceClock, *this, m_clock->getRegAttribs().resetActive);

		reset(synchronizedSignal);
	}

	hlim::Node_Register *Clock::prepRegister(std::string_view name, const RegisterSettings& settings) const
	{
		auto* reg = DesignScope::createNode<hlim::Node_Register>();
		reg->setName(std::string{ name });
		reg->setClock(m_clock);

		if (settings.allowRetimingForward)
			reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD);

		if (settings.allowRetimingBackward)
			reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD);

		EnableScope* scope = EnableScope::get();
		if (scope)
			reg->connectInput(hlim::Node_Register::ENABLE, scope->getFullEnableCondition());
		return reg;
	}

	template<BitVectorDerived T>
	T Clock::buildReg(const T& signal, const RegisterSettings& settings) const
	{
		SignalReadPort data = signal.readPort();

		auto* reg = prepRegister(signal.getName(), settings);
		reg->connectInput(hlim::Node_Register::DATA, data);

		return SignalReadPort(reg, data.expansionPolicy);
	}

	template<BitVectorDerived T>
	T Clock::buildReg(const T& signal, const T& reset, const RegisterSettings& settings) const
	{
		NormalizedWidthOperands ops(signal, reset);

		auto* reg = prepRegister(signal.getName(), settings);
		reg->connectInput(hlim::Node_Register::DATA, ops.lhs);
		reg->connectInput(hlim::Node_Register::RESET_VALUE, ops.rhs);

		return SignalReadPort(reg, ops.lhs.expansionPolicy);
	}

	BVec Clock::operator() (const BVec& signal, const RegisterSettings &settings) const 
	{
		return buildReg(signal, settings);
	}

	BVec Clock::operator() (const BVec& signal, const BVec& reset, const RegisterSettings &settings) const
	{
		return buildReg(signal, reset, settings);
	}

	UInt Clock::operator() (const UInt& signal, const RegisterSettings &settings) const
	{
		return buildReg(signal, settings);
	}

	UInt Clock::operator() (const UInt& signal, const UInt& reset, const RegisterSettings &settings) const
	{
		return buildReg(signal, reset, settings);
	}

	SInt Clock::operator() (const SInt& signal, const RegisterSettings &settings) const
	{
		return buildReg(signal, settings);
	}

	SInt Clock::operator() (const SInt& signal, const SInt& reset, const RegisterSettings &settings) const
	{
		return buildReg(signal, reset, settings);
	}


	Bit Clock::operator()(const Bit& signal, const RegisterSettings& settings) const
	{
		auto* reg = prepRegister(signal.getName(), settings);
		reg->connectInput(hlim::Node_Register::DATA, signal.readPort());

		if (signal.resetValue())
			reg->connectInput(hlim::Node_Register::RESET_VALUE, Bit{ *signal.resetValue() }.readPort());

		Bit ret{ SignalReadPort{reg} };
		if (signal.resetValue())
			ret.resetValue(*signal.resetValue());
		return ret;
	}

	Bit Clock::operator()(const Bit& signal, const Bit& reset, const RegisterSettings& settings) const
	{
		auto* reg = prepRegister(signal.getName(), settings);
		reg->connectInput(hlim::Node_Register::DATA, signal.readPort());
		reg->connectInput(hlim::Node_Register::RESET_VALUE, reset.readPort());

		return SignalReadPort(reg);
	}

	Bit reset()
	{
		const Clock& clk = ClockScope::getClk();
		const auto& attr = clk.getClk()->getRegAttribs();

		Bit ret;
		if (attr.resetType == hlim::RegisterAttributes::ResetType::NONE)
			ret = '0';
		else if(attr.resetActive == hlim::RegisterAttributes::Active::HIGH)
			ret = clk.rstSignal();
		else
			ret = !clk.rstSignal();

		return ret;
	}
}
