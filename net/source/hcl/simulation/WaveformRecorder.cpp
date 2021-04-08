/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "WaveformRecorder.h"

#include "BitAllocator.h"
#include "Simulator.h"

#include "../hlim/Circuit.h"
#include "../hlim/supportNodes/Node_SignalTap.h"
#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/coreNodes/Node_Signal.h"

#include <boost/format.hpp>

namespace hcl::core::sim {

WaveformRecorder::WaveformRecorder(hlim::Circuit &circuit, Simulator &simulator) : m_circuit(circuit), m_simulator(simulator)
{
    m_simulator.addCallbacks(this);
}

void WaveformRecorder::addSignal(hlim::NodePort np, bool hidden, const std::string &nameOverride)
{
    HCL_ASSERT(!hlim::outputIsDependency(np));
    if (!m_signal2id.contains(np)) {
        m_signal2id.insert({np, m_id2Signal.size()});
        Signal signal;
        if (!nameOverride.empty()) {
            signal.name = nameOverride;
        } else {
            std::string baseName = np.node->getName();
            if (baseName.empty())
                baseName = "unnamed";
            signal.name = (boost::format("%s_id_%d") % baseName % np.node->getId()).str();
        }
        signal.isHidden = hidden;
        signal.isBVec = hlim::outputIsBVec(np);
        m_id2Signal.push_back(signal);
    }
}

void WaveformRecorder::addAllWatchSignalTaps()
{
    for (auto &node : m_circuit.getNodes())
        if (auto *tap = dynamic_cast<hlim::Node_SignalTap*>(node.get()))
            if (tap->getLevel() == hlim::Node_SignalTap::LVL_WATCH)
                addSignal(tap->getDriver(0), false, tap->getName());
}

void WaveformRecorder::addAllPins()
{
    for (auto &node : m_circuit.getNodes())
        if (auto *pin = dynamic_cast<hlim::Node_Pin*>(node.get())) {
            auto driver = pin->getDriver(0);
            if (driver.node != nullptr)
                addSignal(driver, false, pin->getName());
            if (!pin->getDirectlyDriven(0).empty())
                addSignal({.node = pin, .port = 0}, false, pin->getName());
        }
}

void WaveformRecorder::addAllOutPins()
{
    for (auto &node : m_circuit.getNodes())
        if (auto *pin = dynamic_cast<hlim::Node_Pin*>(node.get())) {
            auto driver = pin->getDriver(0);
            if (driver.node != nullptr)
                addSignal(driver, false, pin->getName());
        }
}

void WaveformRecorder::addAllNamedSignals(bool appendNodeId)
{
    for (auto &node : m_circuit.getNodes())
        if (auto *sig = dynamic_cast<hlim::Node_Signal*>(node.get())) {
            if (sig->hasGivenName())
                if (!appendNodeId)
                    addSignal({.node=sig, .port=0ull}, false, sig->getName());
                else
                    addSignal({.node=sig, .port=0ull}, false);
        }
}

void WaveformRecorder::addAllSignals(bool appendNodeId)
{
    for (auto &node : m_circuit.getNodes())
        if (auto *sig = dynamic_cast<hlim::Node_Signal*>(node.get())) {
            if (!appendNodeId)
                addSignal({.node=sig, .port=0ull}, !sig->hasGivenName(), sig->getName());
            else
                addSignal({.node=sig, .port=0ull}, !sig->hasGivenName());
        }
}

void WaveformRecorder::initializeStates()
{
    BitAllocator allocator;

    m_id2StateOffsetSize.resize(m_signal2id.size());
    for (auto &sigId : m_signal2id) {
        auto id = sigId.second;
        auto size = sigId.first.node->getOutputConnectionType(sigId.first.port).width;
        auto offset = allocator.allocate((unsigned int)size);
        m_id2StateOffsetSize[id].offset = offset;
        m_id2StateOffsetSize[id].size = size;
    }
    m_trackedState.resize(allocator.getTotalSize());
    m_trackedState.clearRange(DefaultConfig::DEFINED, 0, allocator.getTotalSize());
}


void WaveformRecorder::onNewTick(const hlim::ClockRational &simulationTime)
{
    if (!m_initialized) {
        initializeStates();
        initialize();
        m_initialized = true;
    }

    for (auto &sigId : m_signal2id) {
        auto id = sigId.second;
        auto offset = m_id2StateOffsetSize[id].offset;
        auto size = m_id2StateOffsetSize[id].size;

        auto newState = m_simulator.getValueOfOutput(sigId.first);
        if (newState.size() == 0) continue;

        bool stateChanged = false;
        for (auto p : utils::Range(DefaultConfig::NUM_PLANES)) {
            for (auto i : utils::Range(size))
                if (newState.get(p, i) != m_trackedState.get(p, offset+i)) {
                    stateChanged = true;
                    break;
                }
            if (stateChanged) break;
        }

        if (stateChanged) {
            m_trackedState.copyRange(offset, newState, 0, size);
            signalChanged(id);
        }
    }
    advanceTick(simulationTime);
}



}
