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
#include "ExternalModule.h"
#include "DesignScope.h"

#include <ranges>

using namespace gtry;

ExternalModule::ExternalModule(std::string_view name, std::string_view library) :
	m_node(*DesignScope::get()->getCircuit().createNode<Node_External_Exposed>())
{
	HCL_DESIGNCHECK_HINT(!name.empty(), "module name cannot be empty");

	m_node.name(name);
	if (!library.empty())
		m_node.library(library);
}

GenericParameter& ExternalModule::generic(std::string_view name)
{
	return m_node.generics()[std::string{ name }];
}

const Clock& ExternalModule::clock(std::string_view name, std::optional<std::string_view> resetName, ClockConfig cfg)
{
	auto it = std::ranges::find(m_clock, name, &Clock::name);
	if (it != m_clock.end())
		return m_clock[it - m_clock.end()];

	return addClock(cfg, name, resetName);
}

const Clock& ExternalModule::clock(const Clock& parentClock, std::string_view name, std::optional<std::string_view> resetName, ClockConfig cfg)
{
	auto it = std::ranges::find(m_clock, name, &Clock::name);
	if (it != m_clock.end())
		return m_clock[it - m_clock.end()];

	return addClock(parentClock.deriveClock(cfg), name, resetName);
}

const Clock& gtry::ExternalModule::addClock(Clock clock, std::string_view pinName, std::optional<std::string_view> resetPinName)
{
	Clock& clk = m_clock.emplace_back(clock);
	clk.setName(std::string{ pinName });

	Bit clockSignal; // has to be unassigned for simulation magic :/
	clockSignal.exportOverride(out(pinName));
	clk.overrideClkWith(clockSignal);

	if(resetPinName)
	{
		Bit resetSignal;
		resetSignal.exportOverride(out(*resetPinName));
		clk.overrideRstWith(resetSignal);
	}
	return clk;
}

GenericParameter::BitFlavor gtry::ExternalModule::translateBitType(PinType type)
{
	switch (type)
	{
	case PinType::bit: return GenericParameter::BitFlavor::BIT;
	case PinType::std_logic: return GenericParameter::BitFlavor::STD_LOGIC;
	case PinType::std_ulogic: return GenericParameter::BitFlavor::STD_ULOGIC;
	default: HCL_ASSERT(false);
	}
}

GenericParameter::BitVectorFlavor gtry::ExternalModule::translateBVecType(PinType type)
{
	switch (type)
	{
	case PinType::bit: return GenericParameter::BitVectorFlavor::BIT_VECTOR;
	case PinType::std_logic: return GenericParameter::BitVectorFlavor::STD_LOGIC_VECTOR;
	default: HCL_ASSERT(false);
	}
}

BVec& ExternalModule::in(std::string_view name, BitWidth W, PinConfig cfg)
{
	auto it = std::ranges::find(m_node.ins(), name, &Node_External_Exposed::Port::name);
	if (it == m_node.ins().end())
	{
		BVec& signal = get<BVec>(m_in.emplace_back(W));

		size_t idx = m_node.ins().size();
		m_node.resizeInputs(idx + 1);
		m_node.declInputBitVector(idx, std::string{ name }, W.bits(), {}, translateBVecType(cfg.type));
		m_node.rewireInput(idx, signal.readPort());
		m_node.m_inClock.push_back(ClockScope::getClk().getClk());
		return signal;
	}
	return get<BVec>(m_in[it - m_node.ins().begin()]);
}

Bit& ExternalModule::in(std::string_view name, PinConfig cfg)
{
	auto it = std::ranges::find(m_node.ins(), name, &Node_External_Exposed::Port::name);
	if (it == m_node.ins().end())
	{
		Bit& signal = get<Bit>(m_in.emplace_back());

		size_t idx = m_node.ins().size();
		m_node.resizeInputs(idx + 1);
		m_node.declInputBit(idx, std::string{ name }, translateBitType(cfg.type));
		m_node.rewireInput(idx, signal.readPort());
		m_node.m_inClock.push_back(ClockScope::getClk().getClk());
		return signal;
	}
	return get<Bit>(m_in[it - m_node.ins().begin()]);
}

const BVec& ExternalModule::in(std::string_view name, BitWidth W, PinConfig cfg) const
{
	auto it = std::ranges::find(m_node.ins(), name, &Node_External_Exposed::Port::name);
	HCL_DESIGNCHECK_HINT(it != m_node.ins().end(), "input pin is unknown");
	return get<BVec>(m_in[it - m_node.ins().begin()]);
}

const Bit& ExternalModule::in(std::string_view name, PinConfig cfg) const
{
	auto it = std::ranges::find(m_node.ins(), name, &Node_External_Exposed::Port::name);
	HCL_DESIGNCHECK_HINT(it != m_node.ins().end(), "input pin is unknown");
	return get<Bit>(m_in[it - m_node.ins().begin()]);
}

BVec ExternalModule::out(std::string_view name, BitWidth W, PinConfig cfg)
{
	auto it = std::ranges::find(m_node.outs(), name, &Node_External_Exposed::Port::name);

	size_t idx;
	if (it == m_node.outs().end())
	{
		idx = m_node.outs().size();
		m_node.resizeOutputs(idx + 1);
		m_node.declOutputBitVector(idx, std::string{ name }, W.bits(), {}, translateBVecType(cfg.type));
		m_node.m_outClock.dependentClocks.push_back(
			m_node.clockIndex(ClockScope::getClk().getClk())
		);
	}
	else
	{
		idx = it - m_node.outs().end();
	}
	return SignalReadPort(hlim::NodePort{ .node = &m_node, .port = (size_t)idx });
}

Bit ExternalModule::out(std::string_view name, PinConfig cfg)
{
	auto it = std::ranges::find(m_node.outs(), name, &Node_External_Exposed::Port::name);

	size_t idx;
	if (it == m_node.outs().end())
	{
		idx = m_node.outs().size();
		m_node.resizeOutputs(idx + 1);
		m_node.declOutputBit(idx, std::string{ name }, translateBitType(cfg.type));
		m_node.m_outClock.dependentClocks.push_back(
			m_node.clockIndex(ClockScope::getClk().getClk())
		);
	}
	else
	{
		idx = it - m_node.outs().end();
	}
	return SignalReadPort(hlim::NodePort{ .node = &m_node, .port = (size_t)idx });
}

size_t gtry::ExternalModule::Node_External_Exposed::clockIndex(hlim::Clock* clock)
{
	auto clkit = std::ranges::find(m_clocks, clock);
	if (clkit == m_clocks.end())
	{
		m_clocks.push_back(clock);
		m_clockNames.push_back(""); // TODO: what to put here?
		return m_clocks.size() - 1;
	}
	return clkit - m_clocks.end();
}

std::unique_ptr<hlim::BaseNode> gtry::ExternalModule::Node_External_Exposed::cloneUnconnected() const
{
	auto res = std::make_unique<Node_External_Exposed>();
	copyBaseToClone(res.get());
	return res;
}

hlim::OutputClockRelation gtry::ExternalModule::Node_External_Exposed::getOutputClockRelation(size_t output) const
{
	return m_outClock;
}

bool ExternalModule::Node_External_Exposed::checkValidInputClocks(std::span<hlim::SignalClockDomain> inputClocks) const
{
	HCL_ASSERT(inputClocks.size() == m_inClock.size());

	bool ret = true;
	for (size_t i = 0; i < inputClocks.size(); ++i)
	{
		switch (inputClocks[i].type)
		{
		case hlim::SignalClockDomain::UNKNOWN: ret = false; break;
		case hlim::SignalClockDomain::CLOCK:
			ret &= inputClocks[i].clk->getClockPinSource() == m_inClock[i]->getClockPinSource();
			break;
		default: break;
		}
	}
	return ret;
}
