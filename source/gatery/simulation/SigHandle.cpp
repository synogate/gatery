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

	operator=(state);
}

void SigHandle::operator=(std::int64_t v)
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	DefaultBitVectorState state;
	state.resize(width);
	state.setRange(DefaultConfig::DEFINED, 0, width);
	state.setRange(DefaultConfig::VALUE, 0, width, (v >> 63) & 1); // prefill with sign bit
	if (width)
		state.data(DefaultConfig::VALUE)[0] = v;
	operator=(state);
}

void SigHandle::operator=(std::string_view v)
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	DefaultBitVectorState state = sim::parseBitVector(v);
	HCL_ASSERT(width == state.size());
	operator=(state);
}

void SigHandle::operator=(char v)
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	DefaultBitVectorState state = sim::parseBit(v);
	HCL_ASSERT(width == state.size());
	operator=(state);
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

SigHandle::operator std::int64_t () const
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	if (!width)
		return 0;

	HCL_ASSERT(width <= 64);
	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);

	std::uint64_t res = state.extractNonStraddling(DefaultConfig::VALUE, 0, width);
	if (width < 64) {
		bool negative = res >> (width-1);
		if (negative)
			res |= (~0ull) << width; // sign extend
	}
	return (std::int64_t) res;
}

SigHandle::operator bool () const
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	HCL_ASSERT(width == 1);

	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);

	return state.get(sim::DefaultConfig::VALUE);
}

SigHandle::operator char () const
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	HCL_ASSERT(width == 1);

	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);

	if (!state.get(sim::DefaultConfig::DEFINED))
		return 'x';

	return state.get(sim::DefaultConfig::VALUE)?'1':'0';
}

bool SigHandle::operator==(std::uint64_t v) const
{
	if (!allDefined()) return false;
	return (std::uint64_t)*this == v;
}

bool SigHandle::operator==(std::int64_t v) const
{
	if (!allDefined()) return false;
	return (std::int64_t)*this == v;
}

bool SigHandle::operator==(std::string_view v) const
{
	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);

	ExtendedBitVectorState targetState = sim::parseExtendedBitVector(v);

	HCL_ASSERT(state.size() == targetState.size());

	for (auto i : utils::Range(state.size()))
		if (!targetState.get(sim::ExtendedConfig::DONT_CARE, i)) {

			if (targetState.get(sim::ExtendedConfig::DEFINED, i) != state.get(sim::DefaultConfig::DEFINED, i))
				return false;

			if (targetState.get(sim::ExtendedConfig::DEFINED, i))
				if (targetState.get(sim::ExtendedConfig::VALUE, i) != state.get(sim::DefaultConfig::VALUE, i))
					return false;
		}
	
	return true;
}

bool SigHandle::operator==(char v) const
{
	if (v == '-') return true;

	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);
	HCL_ASSERT(state.size() == 1);

	if (v == 'x' || v == 'X') {
		return state.get(sim::DefaultConfig::DEFINED);
	} else {
		if (!state.get(sim::DefaultConfig::DEFINED)) 
			return false;
			
		if (v == '1')
			return state.get(sim::DefaultConfig::VALUE) == true;
		else
			return state.get(sim::DefaultConfig::VALUE) == false;
	}
}


void SigHandle::overrideDrivingRegister()
{
	if (auto *reg = dynamic_cast<hlim::Node_Register*>(m_output.node))
		m_output = reg->getNonSignalDriver(hlim::Node_Register::DATA);
	
	m_overrideRegister = true;
}

}
