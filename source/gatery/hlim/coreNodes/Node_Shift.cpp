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
#include "Node_Shift.h"

#include <gatery/simulation/BitVectorState.h>

namespace gtry::hlim
{
	Node_Shift::Node_Shift(dir _direction, fill _fill) :
		Node<Node_Shift>(NUM_INPUTS, 1),
		m_direction(_direction),
		m_fill(_fill)
	{
	}

	void Node_Shift::connectOperand(const NodePort& port)
	{
		NodeIO::connectInput(INPUT_OPERAND, port);

		if (port.node)
			setOutputConnectionType(0, port.node->getOutputConnectionType(port.port));
	}

	void Node_Shift::connectAmount(const NodePort& port)
	{
		NodeIO::connectInput(INPUT_AMOUNT, port);
	}

	void Node_Shift::simulateEvaluate(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* inputOffsets, const size_t* outputOffsets) const
	{
		size_t width = getOutputConnectionType(0).width;

		if (inputOffsets[1] == ~0ull) {
			state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], width, false);
			return;
		}
		auto amountDriver = getDriver(1);
		size_t amountWidth = amountDriver.node->getOutputConnectionType(amountDriver.port).width;
		HCL_DESIGNCHECK_HINT(amountWidth <= 64, "no impl");

		uint64_t amountVal = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1], amountWidth);
		uint64_t amountDef = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[1], amountWidth);

		if (amountDef != (1ull << amountWidth) - 1)
		{
			state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], width, false);
			return;
		}

		bool fillVal = false;
		bool fillDef = true;

		switch(m_fill)
		{
		case fill::one:
			fillVal = true;
			break;
		case fill::last:
			if (width && m_direction == dir::left)
			{
				uint64_t lswVal = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], 1);
				uint64_t lswDef = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[0], 1);
				fillVal = lswVal != 0;
				fillDef = lswDef != 0;
			}
			else if (width && m_direction == dir::right)
			{
				uint64_t lswVal = state.extract(sim::DefaultConfig::VALUE, inputOffsets[0] + width - 1, 1);
				uint64_t lswDef = state.extract(sim::DefaultConfig::DEFINED, inputOffsets[0] + width - 1, 1);
				fillVal = lswVal != 0;
				fillDef = lswDef != 0;
			}
			break;
		default: 
			break;
		}

		if (amountVal >= width && m_fill != fill::rotate)
		{
			state.setRange(sim::DefaultConfig::VALUE, outputOffsets[0], width, fillVal);
			state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], width, fillDef);
			return;
		}
		amountVal %= width;

		if (m_direction == dir::left)
		{
			state.copyRange(outputOffsets[0] + amountVal, state, inputOffsets[0] + 0, width - amountVal);

			if (m_fill != fill::rotate)
			{
				state.setRange(sim::DefaultConfig::VALUE, outputOffsets[0], amountVal, fillVal);
				state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], amountVal, fillDef);
			}
			else
			{
				state.copyRange(outputOffsets[0], state, inputOffsets[0] + width - amountVal, amountVal);
			}
		}
		else
		{
			state.copyRange(outputOffsets[0], state, inputOffsets[0] + amountVal, width - amountVal);

			if (m_fill != fill::rotate)
			{
				state.setRange(sim::DefaultConfig::VALUE, outputOffsets[0] + width - amountVal, amountVal, fillVal);
				state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0] + width - amountVal, amountVal, fillDef);
			}
			else
			{
				state.copyRange(outputOffsets[0] + width - amountVal, state, inputOffsets[0], amountVal);
			}
		}
	}

	std::string Node_Shift::getTypeName() const
	{
		std::string name;
		switch (m_fill)
		{
		case fill::zero: name = "LogicShift"; break;
		case fill::one: name = "FillShift"; break;
		case fill::last: name = "ArithmeticShift"; break;
		case fill::rotate: name = "Rotate"; break;
		default:;
		}

		switch (m_direction)
		{
		case dir::left: name += "Left"; break;
		case dir::right: name += "Right"; break;
		}

		return name;
	}

	void Node_Shift::assertValidity() const
	{
	}

	std::string Node_Shift::getInputName(size_t idx) const
	{
		return idx ? "amount" : "in";
	}

	std::string Node_Shift::getOutputName(size_t idx) const
	{
		return "out";
	}




	std::unique_ptr<BaseNode> Node_Shift::cloneUnconnected() const 
	{
		std::unique_ptr<BaseNode> res(new Node_Shift(m_direction, m_fill));
		copyBaseToClone(res.get());
		return res;
	}

}
