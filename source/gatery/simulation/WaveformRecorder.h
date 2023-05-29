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

#include <gatery/utils/StableContainers.h>

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
	class Node_Memory;
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
		virtual ~WaveformRecorder() = default;

		void addSignal(hlim::NodePort driver, hlim::BaseNode *relevantNode, bool hidden);
		void addMemory(hlim::Node_Memory *mem, hlim::NodeGroup *group, const std::string &nameOverride = {}, size_t sortOrder = 0);
		void addAllTaps();
		void addAllPins();
		void addAllOutPins();
		void addAllNamedSignals();
		void addAllSignals();
		void addAllMemories();

		virtual void onAfterPowerOn() override;
		virtual void onCommitState() override;
		virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
	protected:
		hlim::Circuit &m_circuit;
		Simulator &m_simulator;
		bool m_initialized = false;

		/// @brief Keys to deduplicate signals in the waveform.
		/// @details This is supposed to prevent signals from being 
		/// added multiple times, but still allow the same signal to
		/// be added under different names.
		struct SignalReference {
			/// The signal itself
			hlim::RefCtdNodePort driver;
			/// The context (i.e. name, group, ...) in which the signal is considered.
			hlim::NodePtr<hlim::BaseNode> relevantNode;

			auto operator<=>(const SignalReference&) const = default;
		};

		struct StateOffsetSize {
			size_t offset, size;
		};
		struct Signal {
			size_t sortOrder = 0;
			std::string name;
			SignalReference signalRef;
			hlim::Node_Memory *memory = nullptr;
			size_t memoryWordSize = 0;
			size_t memoryWordIdx = 0;
			hlim::NodeGroup *nodeGroup = nullptr;
			bool isBVec;
			bool isHidden;
			bool isPin;
			bool isTap;
		};
		std::vector<StateOffsetSize> m_id2StateOffsetSize;
		std::vector<Signal> m_id2Signal;
		sim::DefaultBitVectorState m_trackedState;
		utils::UnstableMap<SignalReference, size_t> m_alreadyAddedNodePorts;
		utils::UnstableMap<hlim::Node_Memory *, size_t> m_alreadyAddedMemories;

		void initializeStates();
		virtual void initialize() = 0;
		virtual void signalChanged(size_t id) = 0;
		virtual void advanceTick(const hlim::ClockRational &simulationTime) = 0;
};


}
