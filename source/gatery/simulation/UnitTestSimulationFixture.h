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

#include "SimulatorCallbacks.h"
#include "simProc/SimulationProcess.h"

#include <memory>
#include <functional>

namespace gtry::hlim {
	class Circuit;
}

namespace gtry::sim {

	class Simulator;

/**
 * @todo write docs
 */
	class UnitTestSimulationFixture : public SimulatorCallbacks
	{
	public:
		UnitTestSimulationFixture();
		~UnitTestSimulationFixture();

		void addSimulationProcess(std::function<SimulationFunction<>()> simProc);
		void addSimulationFiber(std::function<void()> simFiber);

		void eval(hlim::Circuit& circuit);
		void runTicks(hlim::Circuit& circuit, const hlim::Clock* clock, unsigned numTicks);

		virtual void onDebugMessage(const hlim::BaseNode* src, std::string msg) override;
		virtual void onWarning(const hlim::BaseNode* src, std::string msg) override;
		virtual void onAssert(const hlim::BaseNode* src, std::string msg) override;

		Simulator& getSimulator() { return *m_simulator; }
	protected:
		std::unique_ptr<Simulator> m_simulator;

		const hlim::Clock *m_runLimClock = nullptr;
		unsigned m_runLimTicks = 0;

		std::vector<std::string> m_warnings;
		std::vector<std::string> m_errors;
};


}
