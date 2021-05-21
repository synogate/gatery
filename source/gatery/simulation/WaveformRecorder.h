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

namespace gtry::hlim {
    class Circuit;
    struct NodePort;
    class BaseNode;
}

namespace gtry::sim {

class Simulator;

/**
 * @todo write docs
 */
class WaveformRecorder : public SimulatorCallbacks
{
    public:
        WaveformRecorder(hlim::Circuit &circuit, Simulator &simulator);

        void addSignal(hlim::NodePort np, bool hidden, const std::string &nameOverride = {});
        void addAllWatchSignalTaps();
        void addAllPins();
        void addAllOutPins();
        void addAllNamedSignals(bool appendNodeId = false);
        void addAllSignals(bool appendNodeId = false);

        virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
    protected:
        hlim::Circuit &m_circuit;
        Simulator &m_simulator;
        bool m_initialized = false;

        std::map<hlim::NodePort, size_t> m_signal2id;
        std::vector<hlim::NodePtr<hlim::BaseNode>> m_nodeRefCtrHandles;  /// @todo create ref counting NodePort wrapper

        struct StateOffsetSize {
            size_t offset, size;
        };
        struct Signal {
            std::string name;
            bool isBVec;
            bool isHidden;
        };
        std::vector<StateOffsetSize> m_id2StateOffsetSize;
        std::vector<Signal> m_id2Signal;
        sim::DefaultBitVectorState m_trackedState;

        void initializeStates();
        virtual void initialize() = 0;
        virtual void signalChanged(size_t id) = 0;
        virtual void advanceTick(const hlim::ClockRational &simulationTime) = 0;
};


}
