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

size_t SigHandle::getWidth() const 
{
	return m_output.node->getOutputConnectionType(m_output.port).width;
}

void SigHandle::operator=(std::uint64_t v)
{
	auto width = getWidth();
	ExtendedBitVectorState state;
	state.resize(width);
	state.setRange(ExtendedConfig::DEFINED, 0, width);
	state.clearRange(ExtendedConfig::VALUE, 0, width);
	state.clearRange(ExtendedConfig::DONT_CARE, 0, width);
	state.clearRange(ExtendedConfig::HIGH_IMPEDANCE, 0, width);
	if (width)
		state.data(ExtendedConfig::VALUE)[0] = v;

	operator=(state);
}

void SigHandle::operator=(std::int64_t v)
{
	auto width = getWidth();
	ExtendedBitVectorState state;
	state.resize(width);
	state.setRange(ExtendedConfig::DEFINED, 0, width);
	state.setRange(ExtendedConfig::VALUE, 0, width, (v >> 63) & 1); // prefill with sign bit
	state.clearRange(ExtendedConfig::DONT_CARE, 0, width);
	state.clearRange(ExtendedConfig::HIGH_IMPEDANCE, 0, width);
	if (width)
		state.data(ExtendedConfig::VALUE)[0] = v;
	operator=(state);
}

void SigHandle::operator=(std::string_view v)
{
	auto width = getWidth();
	ExtendedBitVectorState state = sim::parseExtendedBitVector(v);
	HCL_ASSERT(width == state.size());
	operator=(state);
}

void SigHandle::operator=(char v)
{
	auto width = getWidth();
	ExtendedBitVectorState state = sim::parseExtendedBit(v);
	HCL_ASSERT(width == state.size());
	operator=(state);
}

void SigHandle::operator=(const DefaultBitVectorState &state)
{
	if (m_overrideRegister) {
		SimulationContext::current()->overrideRegister(*this, state);
	} else {
		SimulationContext::current()->overrideSignal(*this, sim::convertToExtended(state));
	}
}


void SigHandle::operator=(const ExtendedBitVectorState &state)
{
	if (m_overrideRegister) {
		auto converted = sim::tryConvertToDefault(state);
		HCL_DESIGNCHECK_HINT(converted, "Can not assign dont_care or high_impedance to registers");
		SimulationContext::current()->overrideRegister(*this, *converted);
	} else
		SimulationContext::current()->overrideSignal(*this, state);
}


void SigHandle::operator=(std::span<const std::byte> rhs)
{
	auto width = getWidth();
	HCL_DESIGNCHECK_HINT(width == rhs.size() * 8, "The array that is to be assigned to the simulation signal has the wrong size!");

	if (m_overrideRegister)
		SimulationContext::current()->overrideRegister(*this, sim::createDefaultBitVectorState(rhs.size()*8, rhs.data()));
	else
		SimulationContext::current()->overrideSignal(*this, sim::createExtendedBitVectorState(rhs.size()*8, rhs.data()));	
}

void SigHandle::invalidate()
{
	auto width = getWidth();
	if (m_overrideRegister) {
		DefaultBitVectorState state;
		state.resize(width);
		SimulationContext::current()->overrideRegister(*this, state);
	} else {
		ExtendedBitVectorState state;
		state.resize(width);
		SimulationContext::current()->overrideSignal(*this, state);
	}
}

void SigHandle::stopDriving()
{
	auto width = getWidth();
	if (m_overrideRegister) {
		HCL_DESIGNCHECK_HINT(false, "Can not stop driving registers, as any register output override is anyways only valid until the next clock cycle of that register!");
	} else {
		ExtendedBitVectorState state;
		state.resize(width);
		state.setRange(ExtendedConfig::HIGH_IMPEDANCE, 0, width);
		SimulationContext::current()->overrideSignal(*this, state);
	}
}

std::uint64_t SigHandle::value() const
{
	auto width = getWidth();
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
	auto width = getWidth();
	if (!width)
		return true;

	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);
	return sim::allDefined<DefaultConfig>(state);
}

std::uint64_t SigHandle::defined() const
{
	auto width = getWidth();
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
	auto width = getWidth();
	ExtendedBitVectorState state;
	state.resize(width);
	state.setRange(ExtendedConfig::DEFINED, 0, width);
	sim::insertBigInt(state, 0, width, v);

	(*this) = state;
}

SigHandle::operator sim::BigInt () const
{
	auto state = eval();
	return sim::extractBigInt(state, 0, state.size());
}

SigHandle::operator std::int64_t () const
{
	auto width = getWidth();
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
	auto width = getWidth();
	HCL_ASSERT(width == 1);

	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);

	return state.get(sim::DefaultConfig::VALUE);
}

SigHandle::operator char () const
{
	auto width = getWidth();
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

	for (auto i : utils::Range(state.size())) {
		HCL_DESIGNCHECK_HINT(!targetState.get(sim::ExtendedConfig::HIGH_IMPEDANCE, i), "Can not compare with high impedance, you probably meant to compare with dont_care!");

		if (!targetState.get(sim::ExtendedConfig::DONT_CARE, i)) {

			if (targetState.get(sim::ExtendedConfig::DEFINED, i) != state.get(sim::DefaultConfig::DEFINED, i))
				return false;

			if (targetState.get(sim::ExtendedConfig::DEFINED, i))
				if (targetState.get(sim::ExtendedConfig::VALUE, i) != state.get(sim::DefaultConfig::VALUE, i))
					return false;
		}
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

bool SigHandle::operator==(std::span<const std::byte> rhs) const
{
	auto width = getWidth();
	HCL_DESIGNCHECK_HINT(width == rhs.size()*8, "The array that is to be compared to the simulation signal has the wrong size!");
	
	DefaultBitVectorState state;
	SimulationContext::current()->getSignal(*this, state);

	return state == rhs;
}

void SigHandle::overrideDrivingRegister()
{
	if (auto *reg = dynamic_cast<hlim::Node_Register*>(m_output.node))
		m_output = reg->getNonSignalDriver(hlim::Node_Register::DATA);
	
	m_overrideRegister = true;
}

}
