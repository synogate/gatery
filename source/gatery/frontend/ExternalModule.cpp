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
#include "Pin.h"
#include <gatery/hlim/coreNodes/Node_MultiDriver.h>

#include <ranges>

using namespace gtry;

ExternalModule::ExternalModule(std::string_view name, std::string_view library) :
	m_node(*DesignScope::get()->getCircuit().createNode<Node_External_Exposed>())
{
	HCL_DESIGNCHECK_HINT(!name.empty(), "module name cannot be empty");

	m_node.recordStackTrace();
	m_node.moveToGroup(GroupScope::getCurrentNodeGroup());
	m_node.isEntity(true);

	m_node.name(name);
	if (!library.empty())
		m_node.library(library);
}

GenericParameter& ExternalModule::generic(std::string_view name)
{
	return m_node.generics()[std::string{ name }];
}

void ExternalModule::clockIn(std::string_view name, std::string_view resetName)
{
	clockIn(ClockScope::getClk(), name, resetName);
}

void ExternalModule::clockIn(const Clock& clock, std::string_view name, std::string_view resetName)
{
	m_node.addClock(clock.getClk());
	m_node.clockNames().emplace_back(name);
	m_node.resetNames().emplace_back(resetName);
}


const Clock& ExternalModule::clockOut(std::string_view name, std::optional<std::string_view> resetName, ClockConfig cfg)
{
	auto it = std::ranges::find(m_outClock, name, &Clock::name);
	if (it != m_outClock.end())
		return m_outClock[it - m_outClock.end()];

	return addClockOut(cfg, name, resetName);
}

const Clock& ExternalModule::clockOut(const Clock& parentClock, std::string_view name, std::optional<std::string_view> resetName, ClockConfig cfg)
{
	auto it = std::ranges::find(m_outClock, name, &Clock::name);
	if (it != m_outClock.end())
		return m_outClock[it - m_outClock.end()];

	return addClockOut(parentClock.deriveClock(cfg), name, resetName);
}

const Clock& gtry::ExternalModule::addClockOut(Clock clock, std::string_view pinName, std::optional<std::string_view> resetPinName)
{
	Clock& clk = m_outClock.emplace_back(clock);
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

BVec& ExternalModule::in(std::string_view name, BitWidth W, PinConfig cfg)
{
	auto it = std::ranges::find(m_node.ins(), name, &Node_External_Exposed::Port::name);
	if (it == m_node.ins().end())
	{
		BVec& signal = get<BVec>(m_in.emplace_back(W));

		size_t idx = m_node.ins().size();
		m_node.resizeInputs(idx + 1);
		m_node.declInputBitVector(idx, std::string{ name }, W.bits(), {}, cfg.type);
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
		m_node.declInputBit(idx, std::string{ name }, cfg.type);
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
		m_node.declOutputBitVector(idx, std::string{ name }, W.bits(), {}, cfg.type);
		m_node.m_outClockRelations.push_back({.dependentClocks = {ClockScope::getClk().getClk()}});
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
		m_node.declOutputBit(idx, std::string{ name }, cfg.type);
		m_node.m_outClockRelations.push_back({.dependentClocks = {ClockScope::getClk().getClk()}});
	}
	else
	{
		idx = it - m_node.outs().end();
	}
	return SignalReadPort(hlim::NodePort{ .node = &m_node, .port = (size_t)idx });
}


void ExternalModule::inoutPin(std::string_view portName, std::string_view pinName, BitWidth W, PinConfig cfg)
{
	HCL_DESIGNCHECK_HINT( std::ranges::find(m_node.ins(), portName, &Node_External_Exposed::Port::name) == m_node.ins().end(), "An input port with that name already exists!");
	HCL_DESIGNCHECK_HINT( std::ranges::find(m_node.outs(), portName, &Node_External_Exposed::Port::name) == m_node.outs().end(), "An output port with that name already exists!");

	in(portName, W, cfg);
	out(portName, W, cfg);
	m_node.declareLastIOPairBidir();

	auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .type = hlim::ConnectionType::BITVEC, .width = W.value });
	multiDriver->rewireInput(0, {.node = &m_node, .port = m_node.outs().size()-1});
	m_node.rewireInput(m_node.ins().size()-1, {.node = multiDriver, .port = 0ull});

	Bit ignoredEnable;
	multiDriver->rewireInput(1, BVec(tristatePin(BVec(SignalReadPort(multiDriver)), ignoredEnable).setName(std::string(pinName))).readPort());
}

void ExternalModule::inoutPin(std::string_view portName, std::string_view pinName, PinConfig cfg)
{
	HCL_DESIGNCHECK_HINT( std::ranges::find(m_node.ins(), portName, &Node_External_Exposed::Port::name) == m_node.ins().end(), "An input port with that name already exists!");
	HCL_DESIGNCHECK_HINT( std::ranges::find(m_node.outs(), portName, &Node_External_Exposed::Port::name) == m_node.outs().end(), "An output port with that name already exists!");

	in(portName, cfg);
	out(portName, cfg);
	m_node.declareLastIOPairBidir();

	auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .type = hlim::ConnectionType::BOOL, .width = 1 });
	multiDriver->rewireInput(0, {.node = &m_node, .port = m_node.outs().size()-1});
	m_node.rewireInput(m_node.ins().size()-1, {.node = multiDriver, .port = 0ull});

	Bit ignoredEnable;
	multiDriver->rewireInput(1, Bit(tristatePin(Bit(SignalReadPort(multiDriver)), ignoredEnable).setName(std::string(pinName))).readPort());
}



std::unique_ptr<gtry::hlim::BaseNode> gtry::ExternalModule::Node_External_Exposed::cloneUnconnected() const
{
	auto res = std::make_unique<Node_External_Exposed>();
	copyBaseToClone(res.get());
	return res;
}


void gtry::ExternalModule::Node_External_Exposed::copyBaseToClone(gtry::hlim::BaseNode *copy) const
{
	Node_External::copyBaseToClone(copy);
	auto *other = (gtry::ExternalModule::Node_External_Exposed*)copy;

	other->m_inClock = m_inClock;
	other->m_outClockRelations = m_outClockRelations;
}

hlim::OutputClockRelation gtry::ExternalModule::Node_External_Exposed::getOutputClockRelation(size_t output) const
{
	return m_outClockRelations[output];
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
