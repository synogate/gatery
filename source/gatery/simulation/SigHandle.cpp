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

namespace gtry::sim {

void SigHandle::operator=(std::uint64_t v)
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	HCL_ASSERT(width <= 64);
	DefaultBitVectorState state;
	state.resize(width);
	if (width)
	{
		state.data(DefaultConfig::DEFINED)[0] = 0;
		state.setRange(DefaultConfig::DEFINED, 0, width);
		state.data(DefaultConfig::VALUE)[0] = v;
	}

	SimulationContext::current()->overrideSignal(*this, state);
}

void SigHandle::operator=(const DefaultBitVectorState &state)
{
	SimulationContext::current()->overrideSignal(*this, state);
}

void SigHandle::invalidate()
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	DefaultBitVectorState state;
	state.resize(width);
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

void SigHandle::assign(const BigInt &v)
{
	auto width = m_output.node->getOutputConnectionType(m_output.port).width;
	DefaultBitVectorState state;
	state.resize(width);
	if (width)
	{
		state.setRange(DefaultConfig::DEFINED, 0, width);
		state.clearRange(DefaultConfig::VALUE, 0, width);

		boost::multiprecision::export_bits(
			v, 
			StateBlockOutputIterator{state}, 
			sim::DefaultConfig::NUM_BITS_PER_BLOCK,
			false
		);
	}

	SimulationContext::current()->overrideSignal(*this, state);
}

SigHandle::operator BigInt () const
{
	auto state = eval();
	auto range = state.range(DefaultConfig::VALUE, 0, state.size());

	BigInt result;
	boost::multiprecision::import_bits(
		result, 
		state.data(DefaultConfig::VALUE), 
		state.data(DefaultConfig::VALUE) + state.getNumBlocks(),
		DefaultConfig::NUM_BITS_PER_BLOCK,
		false
	);

	return result;
}



}
