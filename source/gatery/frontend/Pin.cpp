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
#include "Pin.h"

#include "Bit.h"
#include "UInt.h"

#include "Scope.h"
#include "Clock.h"

namespace gtry {

	BaseOutputPin::BaseOutputPin(hlim::NodePort nodePort, std::string name)
	{
		m_pinNode = DesignScope::createNode<hlim::Node_Pin>(false, true, false);
		m_pinNode->connect(nodePort);
		m_pinNode->setName(std::move(name));
		if (ClockScope::anyActive())
			m_pinNode->setClockDomain(ClockScope::getClk().getClk());
	}

	OutputPin::OutputPin(const Bit &bit) : BaseOutputPin(bit.readPort(), std::string(bit.getName())) { }




	BaseInputPin::BaseInputPin() {
		m_pinNode = DesignScope::createNode<hlim::Node_Pin>(true, false, false);
		if (ClockScope::anyActive())
			m_pinNode->setClockDomain(ClockScope::getClk().getClk());
	}

	InputPin::InputPin() {
		m_pinNode->setBool();
	}


	InputPin::operator Bit () const
	{
		return Bit(SignalReadPort({.node=m_pinNode, .port=0ull}));
	}

	InputPins::InputPins(BitWidth width) {
		m_pinNode->setWidth(width.value);
	}

	InputPins::operator UInt () const { return UInt(SignalReadPort({.node=m_pinNode, .port=0ull})); }



	BaseTristatePin::BaseTristatePin(hlim::NodePort nodePort, std::string name, const Bit &outputEnable) {
		m_pinNode = DesignScope::createNode<hlim::Node_Pin>(true, true, true);
		m_pinNode->connect(nodePort);
		m_pinNode->connectEnable(outputEnable.readPort());
		m_pinNode->setName(std::move(name));
		if (ClockScope::anyActive())
			m_pinNode->setClockDomain(ClockScope::getClk().getClk());
	}

	TristatePin::TristatePin(const Bit &bit, const Bit &outputEnable) : BaseTristatePin(bit.readPort(), std::string(bit.getName()), outputEnable) 
	{
	 
	}


	TristatePin::operator Bit () const
	{
		return Bit(SignalReadPort({.node=m_pinNode, .port=0ull}));
	}


	TristatePins::operator UInt () const 
	{ 
		return UInt(SignalReadPort({.node=m_pinNode, .port=0ull})); 
	}


	OutputPins pinOut(const InputPins &input) { return OutputPins((UInt)input); }

}
