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
#include "BitVectorState.h"
#include "../hlim/NodePtr.h"

#include <vector>
#include <map>
#include <set>

namespace gtry::hlim {
	class Circuit;
	struct NodePort;
	class BaseNode;
	class NodeGroup;
}

namespace gtry::sim {

class Simulator;

/**
 * @brief Base class for waveform recorders (e.g. to write VCD files of a simulation run).
 */
class WaveformRecorder : public SimulatorCallbacks
{
	public:
		WaveformRecorder(hlim::Circuit &circuit, Simulator &simulator);

		void addSignal(hlim::NodePort np, bool isPin, bool hidden, hlim::NodeGroup *group, const std::string &nameOverride = {}, size_t sortOrder = 0);
		void addAllWatchSignalTaps();
		void addAllPins();
		void addAllOutPins();
		void addAllNamedSignals(bool appendNodeId = false);
		void addAllSignals(bool appendNodeId = false);

		virtual void onAfterPowerOn() override;
		virtual void onCommitState() override;
		virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
	protected:
		hlim::Circuit &m_circuit;
		Simulator &m_simulator;
		bool m_initialized = false;

		struct StateOffsetSize {
			size_t offset, size;
		};
		struct Signal {
			size_t sortOrder = 0;
			std::string name;
			hlim::RefCtdNodePort driver;
			hlim::NodeGroup *nodeGroup = nullptr;
			bool isBVec;
			bool isHidden;
			bool isPin;
		};
		std::vector<StateOffsetSize> m_id2StateOffsetSize;
		std::vector<Signal> m_id2Signal;
		sim::DefaultBitVectorState m_trackedState;
		std::set<hlim::NodePort> m_alreadyAddedNodePorts;

		void initializeStates();
		virtual void initialize() = 0;
		virtual void signalChanged(size_t id) = 0;
		virtual void advanceTick(const hlim::ClockRational &simulationTime) = 0;
};


}
