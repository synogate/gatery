#pragma once

#include "SimulatorCallbacks.h"
#include "BitVectorState.h"

#include <vector>
#include <map>

namespace hcl::core::hlim {
    class Circuit;
    class NodePort;
}

namespace hcl::core::sim {

class Simulator;

/**
 * @todo write docs
 */
class WaveformRecorder : public SimulatorCallbacks
{
    public:
        WaveformRecorder(hlim::Circuit &circuit, Simulator &simulator);
        
        void addSignal(hlim::NodePort np);
        void addAllWatchSignalTaps();
        void addAllOutPins();
        void addAllNamedSignals();

        virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
    protected:
        hlim::Circuit &m_circuit;
        Simulator &m_simulator;
        bool m_initialized = false;

        std::map<hlim::NodePort, size_t> m_signal2id;
        struct StateOffsetSize {
            size_t offset, size;
        };
        std::vector<StateOffsetSize> m_id2StateOffsetSize;
        sim::DefaultBitVectorState m_trackedState;

        void initializeStates();
        virtual void initialize() = 0;
        virtual void signalChanged(size_t id) = 0;
        virtual void advanceTick(const hlim::ClockRational &simulationTime) = 0;
};


}
