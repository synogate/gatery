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

#include "ConditionalScope.h"
#include "trace.h"

#include <gatery/hlim/coreNodes/Node_Clk2Signal.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

#include <boost/algorithm/string.hpp>

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
			setAbsoluteFrequency(clockFromString(config.as<std::string>()));
		else
		{
			if (config["period"])
				setAbsoluteFrequency(clockFromString(config["period"].as<std::string>()));

			if (config["clock_edge"])
				m_triggerEvent = config["clock_edge"].as<TriggerEvent>();

			if (config["reset_name"])
				m_resetName = config["reset_name"].as<std::string>();

			if (config["reset_type"])
				m_resetType = config["reset_type"].as<ResetType>();

			if (config["memory_reset_type"])
				m_memoryResetType = config["memory_reset_type"].as<ResetType>();

			if (config["reset_active"])
				m_resetActive = config["reset_active"].as<ResetActive>();

			if (config["initialize_registers"])
				m_initializeRegs = config["initialize_registers"].as<bool>();
		
			if (config["initialize_memories"])
				m_initializeMemory = config["initialize_memories"].as<bool>();

			m_attributes.loadConfig(config["attributes"]);
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

		if (m_name)
			s << *m_name;
		
		if (m_absoluteFrequency)
		{
			double period_mhz = double(m_absoluteFrequency->numerator()) / m_absoluteFrequency->denominator();
			period_mhz /= 1'000'000;
			std::string text = std::to_string(size_t(std::round(period_mhz))) + " MHz";
			print("period", std::make_optional(text));
		}

		print("clock edge", m_triggerEvent);
		print("reset name", m_resetName);
		print("reset type", m_resetType);
		print("memory reset type", m_memoryResetType);
		print("reset active", m_resetActive);
		print("initialize registers", m_initializeRegs);
		print("initialize memory", m_initializeMemory);

		auto it = m_attributes.userDefinedVendorAttributes.find("all");
		if(it != m_attributes.userDefinedVendorAttributes.end())
			for (const auto& attr : it->second)
				s << ", " << attr.first << ": " << attr.second.value;

	}


	Clock::Clock(size_t freq) : Clock(ClockConfig{}.setAbsoluteFrequency({ freq, 1 }))
	{
	}

	Clock::Clock(const ClockConfig& config)
	{
		HCL_DESIGNCHECK_HINT(config.m_absoluteFrequency, "A root clock must have an absolute frequency defined through it's ClockConfig!");
		HCL_DESIGNCHECK_HINT(!config.m_frequencyMultiplier, "A root clock mustmust not have a parent relative frequency multiplier defined through it's ClockConfig!");
		m_clock = DesignScope::createClock<hlim::RootClock>(config.m_name ? *config.m_name : std::string("sysclk"), *config.m_absoluteFrequency);
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
		if (config.m_absoluteFrequency) {
			HCL_ASSERT_HINT(false, "Absolute frequencies on derived clocks not implemented yet!");
		}
		if (config.m_frequencyMultiplier) dynamic_cast<hlim::DerivedClock*>(m_clock)->setFrequencyMuliplier(*config.m_frequencyMultiplier);
		applyConfig(config);

	}

	void Clock::applyConfig(const ClockConfig& config)
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

		if (config.m_resetActive) m_clock->getRegAttribs().resetActive = *config.m_resetActive;

		if (config.m_registerEnablePinUsage) m_clock->getRegAttribs().registerEnablePinUsage = *config.m_registerEnablePinUsage;
		if (config.m_registerResetPinUsage) m_clock->getRegAttribs().registerResetPinUsage = *config.m_registerResetPinUsage;

		HCL_DESIGNCHECK_HINT(m_clock->getRegAttribs().resetType != ResetType::NONE || m_clock->getRegAttribs().initializeRegs, "Either a type of reset, or the initialization for registers should be enabled!");

		for (const auto& vendor : config.m_attributes.userDefinedVendorAttributes)
			for (const auto& attrib : vendor.second)
				m_clock->getRegAttribs().userDefinedVendorAttributes[vendor.first][attrib.first] = attrib.second;
	}



	Clock Clock::deriveClock(const ClockConfig& config)
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

	hlim::Node_Register *Clock::prepRegister(std::string_view name, const RegisterSettings& settings) const
	{
		auto* reg = DesignScope::createNode<hlim::Node_Register>();
		reg->setName(std::string{ name });
		reg->setClock(m_clock);

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
		return reg;
	}

	template<BitVectorDerived T>
	T Clock::buildReg(const T& signal, const RegisterSettings& settings) const
	{
		SignalReadPort data = signal.getReadPort();

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
		reg->connectInput(hlim::Node_Register::DATA, signal.getReadPort());

		if (signal.getResetValue())
			reg->connectInput(hlim::Node_Register::RESET_VALUE, Bit{ *signal.getResetValue() }.getReadPort());

		Bit ret{ SignalReadPort{reg} };
		if (signal.getResetValue())
			ret.setResetValue(*signal.getResetValue());
		return ret;
	}

	Bit Clock::operator()(const Bit& signal, const Bit& reset, const RegisterSettings& settings) const
	{
		auto* reg = prepRegister(signal.getName(), settings);
		reg->connectInput(hlim::Node_Register::DATA, signal.getReadPort());
		reg->connectInput(hlim::Node_Register::RESET_VALUE, reset.getReadPort());

		return SignalReadPort(reg);
	}


}
