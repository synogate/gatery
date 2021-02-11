#include "WaveformRecorder.h"

#include "BitAllocator.h"
#include "Simulator.h"

#include "../hlim/Circuit.h"
#include "../hlim/supportNodes/Node_SignalTap.h"
#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/coreNodes/Node_Signal.h"

namespace hcl::core::sim {

WaveformRecorder::WaveformRecorder(hlim::Circuit &circuit, Simulator &simulator) : m_circuit(circuit), m_simulator(simulator)
{
    m_simulator.addCallbacks(this);
}

void WaveformRecorder::addSignal(hlim::NodePort np)
{
    HCL_ASSERT(np.node->getOutputConnectionType(np.port).interpretation != hlim::ConnectionType::DEPENDENCY);
    if (!m_signal2id.contains(np))
        m_signal2id.insert({np, m_signal2id.size()});
}

void WaveformRecorder::addAllWatchSignalTaps()
{
    for (auto &node : m_circuit.getNodes())
        if (auto *tap = dynamic_cast<hlim::Node_SignalTap*>(node.get()))
            if (tap->getLevel() == hlim::Node_SignalTap::LVL_WATCH)
                addSignal(tap->getDriver(0));
}

void WaveformRecorder::addAllOutPins()
{
    for (auto &node : m_circuit.getNodes())
        if (auto *pin = dynamic_cast<hlim::Node_Pin*>(node.get())) {
            auto driver = pin->getDriver(0);
            if (driver.node != nullptr)
                addSignal(driver);
        }
}

void WaveformRecorder::addAllNamedSignals()
{
    for (auto &node : m_circuit.getNodes())
        if (auto *sig = dynamic_cast<hlim::Node_Signal*>(node.get())) {
            if (!sig->getName().empty())
                addSignal({.node=sig, .port=0ull});
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
