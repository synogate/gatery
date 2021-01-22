#include "Node_Shift.h"

namespace hcl::core::hlim
{
	Node_Shift::Node_Shift(dir _direction, fill _fill) :
		Node<Node_Shift>(2, 1),
		m_direction(_direction),
		m_fill(_fill)
	{
	}

	void Node_Shift::connectOperand(const NodePort& port)
	{
		NodeIO::connectInput(0, port);

		if (port.node)
			setOutputConnectionType(0, port.node->getOutputConnectionType(port.port));
	}

	void Node_Shift::connectAmount(const NodePort& port)
	{
		NodeIO::connectInput(1, port);
	}

	void Node_Shift::simulateEvaluate(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* inputOffsets, const size_t* outputOffsets) const
	{
		size_t width = getOutputConnectionType(0).width;

		NodePort amountDriver = getNonSignalDriver(1);
		if (!amountDriver.node)
		{
			state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], width, false);
			return;
		}
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
}
