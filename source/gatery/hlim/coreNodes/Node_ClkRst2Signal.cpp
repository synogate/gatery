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
#include "Node_ClkRst2Signal.h"
#include "../Clock.h"

#include <gatery/simulation/BitVectorState.h>

namespace gtry::hlim {

Node_ClkRst2Signal::Node_ClkRst2Signal() : Node(0, 1)
{
	ConnectionType conType;
	conType.width = 1;
	conType.type = ConnectionType::BOOL;
	setOutputConnectionType(0, conType);

	m_clocks.resize(1);
	setOutputType(0, OUTPUT_LATCHED);	
}

void Node_ClkRst2Signal::simulateResetChange(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort, bool resetHigh) const
{
	state.set(sim::DefaultConfig::DEFINED, outputOffsets[0], true);
	state.set(sim::DefaultConfig::VALUE, outputOffsets[0], resetHigh);
}


std::string Node_ClkRst2Signal::getTypeName() const
{
	return "clkrst2signal";
}

void Node_ClkRst2Signal::assertValidity() const
{
}

std::string Node_ClkRst2Signal::getInputName(size_t idx) const
{
	return "";
}

std::string Node_ClkRst2Signal::getOutputName(size_t idx) const
{
	return "rst";
}

void Node_ClkRst2Signal::setClock(Clock *clk)
{
	attachClock(clk, 0);
}


std::unique_ptr<BaseNode> Node_ClkRst2Signal::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_ClkRst2Signal());
	copyBaseToClone(res.get());
	return res;
}

std::string Node_ClkRst2Signal::attemptInferOutputName(size_t outputPort) const
{
	std::stringstream name;
	name << m_clocks[0]->getResetName();
	return name.str();
}

}
