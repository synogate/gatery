#include "Node_MemWritePort.h"

#include "Node_Memory.h"

namespace hcl::core::hlim {

Node_MemWritePort::Node_MemWritePort(std::size_t bitWidth) : m_bitWidth(bitWidth)
{
    resizeInputs((unsigned)Inputs::count);
    resizeOutputs(0);
    m_clocks.resize(1);
}

void Node_MemWritePort::connectMemory(Node_Memory *memory)
{
    connectInput((unsigned)Inputs::memory, {.node=memory, .port=0});
}

void Node_MemWritePort::disconnectMemory()
{
    disconnectInput((unsigned)Inputs::memory);
}


Node_Memory *Node_MemWritePort::getMemory()
{
    return dynamic_cast<Node_Memory *>(getDriver((unsigned)Inputs::memory).node);
}

void Node_MemWritePort::connectEnable(const NodePort &output)
{
    connectInput((unsigned)Inputs::enable, output);
}

void Node_MemWritePort::connectAddress(const NodePort &output)
{
    connectInput((unsigned)Inputs::address, output);
}

void Node_MemWritePort::connectData(const NodePort &output)
{
    connectInput((unsigned)Inputs::data, output);
}

bool Node_MemWritePort::hasSideEffects() const
{
    return getDriver((unsigned)Inputs::memory).node != nullptr;
}

void Node_MemWritePort::setClock(Clock* clk)
{
    attachClock(clk, 0);
}

void Node_MemWritePort::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
}

void Node_MemWritePort::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
}



std::string Node_MemWritePort::getTypeName() const
{
    return "mem_write_port";
}

void Node_MemWritePort::assertValidity() const
{
}

std::string Node_MemWritePort::getInputName(size_t idx) const
{
    switch (idx) {
        case (unsigned)Inputs::memory:
            return "memory";
        case (unsigned)Inputs::address:
            return "addr";
        case (unsigned)Inputs::enable:
            return "enable";
        case (unsigned)Inputs::data:
            return "data";
        default:
            return "unknown";
    }
}

std::string Node_MemWritePort::getOutputName(size_t idx) const
{
    return "";
}


std::vector<size_t> Node_MemWritePort::getInternalStateSizes() const
{
    return {};
}

}
