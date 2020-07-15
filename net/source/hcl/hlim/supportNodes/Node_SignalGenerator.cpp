#include "Node_SignalGenerator.h"

#include "../../utils/Range.h"

namespace hcl::core::hlim {

Node_SignalGenerator::Node_SignalGenerator(Clock *clk) {
    m_clocks.resize(1);
    attachClock(clk, 0);
}

void Node_SignalGenerator::setOutputs(const std::vector<ConnectionType> &connections)
{
    resizeOutputs(connections.size());
    for (auto i : utils::Range(connections.size())) {
        setOutputConnectionType(i, connections[i]);
        setOutputType(i, OUTPUT_LATCHED);
    }
}

void Node_SignalGenerator::resetDataDefinedZero(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const
{
    for (auto i : utils::Range(getNumOutputPorts())) {
        state.setRange(sim::DefaultConfig::VALUE, outputOffsets[i], getOutputConnectionType(i).width, false);
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[i], getOutputConnectionType(i).width, true);
    }
}

void Node_SignalGenerator::resetDataUndefinedZero(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const
{
    for (auto i : utils::Range(getNumOutputPorts())) {
        state.setRange(sim::DefaultConfig::VALUE, outputOffsets[i], getOutputConnectionType(i).width, false);
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[i], getOutputConnectionType(i).width, false);
    }
}

void Node_SignalGenerator::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
    std::uint64_t &tick = state.data(sim::DefaultConfig::VALUE)[internalOffsets[0]/64];
    tick = 0;
    produceSignals(state, outputOffsets, tick);
}

void Node_SignalGenerator::simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
{
    std::uint64_t &tick = state.data(sim::DefaultConfig::VALUE)[internalOffsets[0]/64];
    tick++;
    produceSignals(state, outputOffsets, tick);
}

std::string Node_SignalGenerator::getTypeName() const 
{ 
    return "SignalGenerator";
}

void Node_SignalGenerator::assertValidity() const 
{ 

}

std::string Node_SignalGenerator::getInputName(size_t idx) const 
{
    return "";
}

std::vector<size_t> Node_SignalGenerator::getInternalStateSizes() const 
{
    return {64};
}

}
