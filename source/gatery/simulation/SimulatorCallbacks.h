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

#include "../hlim/ClockRational.h"
#include "../hlim/NodePort.h"
#include "BitVectorState.h"

#include <string>

namespace gtry::hlim {
	class Clock;
	class BaseNode;
}

namespace gtry::sim {

/**
 * @brief Interface for classes that want to be informed of simulator events.
 */
class SimulatorCallbacks
{
	public:
		virtual ~SimulatorCallbacks() = default;

		virtual void onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc) { }
		virtual void onAnnotationEnd(const hlim::ClockRational &simulationTime, const std::string &id) { }

		/**
		 * @brief Called immediately when the simulation is powered on before any initialization has happened.
		 */
		virtual void onPowerOn() { }

		/**
		 * @brief Called after the simulation has powered on but before simulation processes have started.
		 * @details Registers and memories have potentially attained their initialization values, but the reset is potentially still in progress.
		 */
		virtual void onAfterPowerOn() { }

		/**
		 * @brief Called whenever combinatorial signals have stabilized.
		 * @details This is where checks can be performed or states can be written to waveform files.
		 */
		virtual void onCommitState() { }

		/**
		 * @brief Called whenever the simulation time advances, but before the new state for this time step has been evaluated.
		 * @param simulationTime The new simulator time.
		 */
		virtual void onNewTick(const hlim::ClockRational &simulationTime) { }

		/**
		 * @brief Called whenever the simulator switched to a new phase within a simulation tick (Before, During, or After registers at that simulation tick trigger).
		 * @param phase The new simulator phase.
		 */
		virtual void onNewPhase(size_t phase) { }


		/**
		 * @brief Called whenever the simulation finished evaluating micro tick but before having commited the state since another micro tick might be necessary.
		 * @param microTick The micro tick that was just finished.
		 */
		virtual void onAfterMicroTick(size_t microTick) { }

		/**
		 * @brief Called when a clock changes its value (twice per clock cycle).
		 * @param clock The clock whose value is changing. Does not trigger for inherited clocks that only change attributes.
		 * @param risingEdge Wether the new clock value is asserted.
		 * @note A rising edge is not necessarily a clock activation. Registers can also be configured to trigger on falling (or on both) edges.
		 */
		virtual void onClock(const hlim::Clock *clock, bool risingEdge) { }

		/**
		 * @brief Called when a reset changes its value (gets asserted or de-asserted).
		 * @details On powerOn, the initial assertion of resets also triggers this event before the onPowerOn event.
		 * @param clock The clock whose reset is changing. Does not trigger for inherited clocks that only change attributes.
		 * @param resetAsserted Wether the new reset value is asserted.
		 * @note An asserted reset is not necessarily an active reset. Registers can also be configured to reset on a de-asserted reset signal.
		 */
		virtual void onReset(const hlim::Clock *clock, bool resetAsserted) { }

		virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) { }
		virtual void onWarning(const hlim::BaseNode *src, std::string msg) { }
		virtual void onAssert(const hlim::BaseNode *src, std::string msg) { }

		/**
		 * @brief Called when a signal (e.g. an input pin) gets a value assigned by a simulation process.
		 * @details Test bench exporters can use this events to note the new assigned value.
		 * @param output The signal that is overridden.
		 * @param state The new value.
		 */
		virtual void onSimProcOutputOverridden(const hlim::NodePort &output, const ExtendedBitVectorState &state) { }

		/**
		 * @brief Called when a signal is read by a simulation process.
		 * @details Test bench exporters can use this events to export asserts. Whenever a value is being read during gatery simulation,
		 * we assume that it is checked or used by a unit test. If the unit test passes, then all read values are deemed "correct" and
		 * should have the same value in an external simulator.
		 * @note Since gatery is more strict about undefined values than e.g. vhdl simulators,
		 * undefined values that are defined in an external simulator should still be deemed correct.
		 * @param output The signal that was read.
		 * @param state The value that was retrieved.
		 */
		virtual void onSimProcOutputRead(const hlim::NodePort &output, const DefaultBitVectorState &state) { }
	protected:
};


/**
 * @brief Simple SimulatorCallbacks implementation that writes the most important events to the console.
 */
class SimulatorConsoleOutput : public SimulatorCallbacks
{
	public:
		virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
		virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
		virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
		virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
		virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
	protected:
		hlim::ClockRational m_simTime;
};


}
