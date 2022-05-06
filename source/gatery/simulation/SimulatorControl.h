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
#pragma once

#include "Simulator.h"
#include "BitVectorState.h"

#include "../hlim/NodeIO.h"

#include <functional>
#include <vector>
#include <string>

namespace gtry::sim {

class SimulatorControl
{
	public:
		struct StateBreakpoint {
			enum Trigger {
				TRIG_CHANGE,
				TRIG_EQUAL,
				TRIG_NOT_EQUAL
			};
			Trigger trigger;
			DefaultBitVectorState refValue;
			
			size_t stateOffset;
		};
		
		struct SignalBreakpoint : StateBreakpoint {
			hlim::NodePort nodePort;
		};
		
		void bindSimulator(Simulator *simulator);
		
		

		void advanceAnyTick();
		void advanceTick(const std::string &clk);
		void freeRun(const std::function<bool()> &tickCallback, std::vector<size_t> &triggeredBreakpoints);
	protected:
		Simulator *m_simulator = nullptr;
		std::vector<SignalBreakpoint> m_signalBreakpoints;
};

}
