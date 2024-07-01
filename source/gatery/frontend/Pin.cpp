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

namespace gtry
{
	BaseOutputPin::BaseOutputPin(hlim::NodePort nodePort, std::string name, const PinNodeParameter& params)
	{
		m_pinNode = DesignScope::createNode<hlim::Node_Pin>(false, true, false);
		m_pinNode->connect(nodePort);
		m_pinNode->setName(std::move(name));
		m_pinNode->setPinNodeParameter(params);
		if (ClockScope::anyActive())
			m_pinNode->setClockDomain(ClockScope::getClk().getClk());
	}

	OutputPin::OutputPin(const Bit& bit, const PinNodeParameter& params) : BaseOutputPin(bit.readPort(), std::string(bit.getName()), params) { }




	BaseInputPin::BaseInputPin() {
		m_pinNode = DesignScope::createNode<hlim::Node_Pin>(true, false, false);
		if (ClockScope::anyActive())
			m_pinNode->setClockDomain(ClockScope::getClk().getClk());
	}

	InputPin::InputPin(const PinNodeParameter& params) {
		m_pinNode->setBool();
		m_pinNode->setPinNodeParameter(params);
	}


	InputPin::operator Bit () const
	{
		return Bit(SignalReadPort({ .node = m_pinNode, .port = 0ull }));
	}

	InputPins::InputPins(BitWidth width, const PinNodeParameter& params) {
		m_pinNode->setWidth(width.value);
		m_pinNode->setPinNodeParameter(params);
	}

	InputPins::operator UInt () const { return UInt(SignalReadPort({ .node = m_pinNode, .port = 0ull })); }



	BaseTristatePin::BaseTristatePin(hlim::NodePort nodePort, std::string name, const Bit& outputEnable, const PinNodeParameter& params) {
		m_pinNode = DesignScope::createNode<hlim::Node_Pin>(true, true, true);
		m_pinNode->setPinNodeParameter(params);
		m_pinNode->connect(nodePort);
		m_pinNode->connectEnable(outputEnable.readPort());
		m_pinNode->setName(std::move(name));
		if (ClockScope::anyActive())
			m_pinNode->setClockDomain(ClockScope::getClk().getClk());
	}

	TristatePin::TristatePin(const Bit& bit, const Bit& outputEnable, const PinNodeParameter& params) : BaseTristatePin(bit.readPort(), std::string(bit.getName()), outputEnable, params)
	{

	}


	TristatePin::operator Bit () const
	{
		return Bit(SignalReadPort({ .node = m_pinNode, .port = 0ull }));
	}


	TristatePins::operator UInt () const
	{
		return UInt(SignalReadPort({ .node = m_pinNode, .port = 0ull }));
	}




	BaseBidirPin::BaseBidirPin(hlim::NodePort nodePort, std::string name) {
		m_pinNode = DesignScope::createNode<hlim::Node_Pin>(true, true, true);
		m_pinNode->connect(nodePort);
		m_pinNode->setName(std::move(name));
		if (ClockScope::anyActive())
			m_pinNode->setClockDomain(ClockScope::getClk().getClk());
	}

	BidirPin::BidirPin(const Bit& bit) : BaseBidirPin(bit.readPort(), std::string(bit.getName()))
	{

	}

	BidirPin::operator Bit () const
	{
		return Bit(SignalReadPort({ .node = m_pinNode, .port = 0ull }));
	}


	BidirPins::operator UInt () const
	{
		return UInt(SignalReadPort({ .node = m_pinNode, .port = 0ull }));
	}

	OutputPins pinOut(const InputPins& input, const PinNodeParameter& params) { return OutputPins((UInt)input, params); }

	void internal::PinVisitor::reverse()
	{
		isReverse = !isReverse;
	}

	void internal::PinVisitor::elementaryOnly(ElementarySignal& vec)
	{
		auto name = makeName();
		HCL_DESIGNCHECK_HINT(vec.valid(), "Can not pin uninitialized bitvectors but the member " + name + " is uninitialized!");

		hlim::Node_Pin* const node = DesignScope::createNode<hlim::Node_Pin>(!isReverse, isReverse, false);
		if (isReverse)
		{
			node->connect(vec.readPort());
			if (ClockScope::anyActive())
				node->setClockDomain(ClockScope::getClk().getClk());
		}
		else
		{
			if (vec.connType().isBool())
				node->setBool();
			else
				node->setWidth(vec.width().bits());

			vec.assign(SignalReadPort{ node });
		}

		node->setName(makeName());
		node->setPinNodeParameter(*params);
		if (ClockScope::anyActive())
			node->setClockDomain(ClockScope::getClk().getClk());
	}
}
