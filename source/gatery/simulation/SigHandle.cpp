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
#include "SigHandle.h"

#include "SimulationContext.h"
#include "../hlim/Node.h"
#include "../hlim/coreNodes/Node_Register.h"

namespace gtry::sim {

void SigHandle::operator=(std::uint64_t v)
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	DefaultBitVectorState state;
	state.resize(width);
	state.setRange(DefaultConfig::DEFINED, 0, width);
	state.setRange(DefaultConfig::VALUE, 0, width);
	if (width)
		state.data(DefaultConfig::VALUE)[0] = v;

	if (m_overrideRegister)
		SimulationContext::current()->overrideRegister(*this, state);
	else
		SimulationContext::current()->overrideSignal(*this, state);
}

void SigHandle::operator=(const DefaultBitVectorState &state)
{
	if (m_overrideRegister)
		SimulationContext::current()->overrideRegister(*this, state);
	else
		SimulationContext::current()->overrideSignal(*this, state);
}

void SigHandle::invalidate()
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	DefaultBitVectorState state;
	state.resize(width);
	if (m_overrideRegister)
		SimulationContext::current()->overrideRegister(*this, state);
	else
		SimulationContext::current()->overrideSignal(*this, state);
}


std::uint64_t SigHandle::value() const
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	if (!width)
		return 0;

	HCL_ASSERT(width <= 64);
	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);
	//HCL_ASSERT(sim::allDefinedNonStraddling(state, 0, width));
	return state.extractNonStraddling(DefaultConfig::VALUE, 0, width);
}

DefaultBitVectorState SigHandle::eval() const
{
	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);
	return state;
}

bool SigHandle::allDefined() const
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	if (!width)
		return true;

	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);
	return sim::allDefined<DefaultConfig>(state);
}

std::uint64_t SigHandle::defined() const
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	if (!width)
		return 0;

	HCL_ASSERT(width <= 64);
	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);
	return state.extractNonStraddling(DefaultConfig::DEFINED, 0, width);
}


struct StateBlockOutputIterator {
	StateBlockOutputIterator(DefaultBitVectorState &state) : state(state) { }

	void operator++() { offset++; }
	auto &operator*() { HCL_ASSERT(offset < state.getNumBlocks()); return state.data(DefaultConfig::VALUE)[offset]; }

	DefaultBitVectorState &state;
	size_t offset = 0;
};

void SigHandle::assign(const sim::BigInt &v)
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	DefaultBitVectorState state;
	state.resize(width);
	state.setRange(DefaultConfig::DEFINED, 0, width);
	sim::insertBigInt(state, 0, width, v);

	if (m_overrideRegister)
		SimulationContext::current()->overrideRegister(*this, state);
	else
		SimulationContext::current()->overrideSignal(*this, state);
}

SigHandle::operator sim::BigInt () const
{
	auto state = eval();
	return sim::extractBigInt(state, 0, state.size());
}


SigHandle SigHandle::drivingReg() const
{
	if (auto *reg = dynamic_cast<hlim::Node_Register*>(m_output.node)) {
		return { reg->getNonSignalDriver(hlim::Node_Register::DATA), true };
	} else {
		return { m_output, true };
	}
}

}
