#include "Node_MemPort.h"

#include "Node_Memory.h"

namespace hcl::core::hlim {

Node_MemPort::Node_MemPort(std::size_t bitWidth) : m_bitWidth(bitWidth)
{
    resizeInputs((unsigned)Inputs::count);
    resizeOutputs((unsigned)Outputs::count);
    setOutputConnectionType((unsigned)Outputs::rdData, {.interpretation = ConnectionType::BITVEC, .width=bitWidth});
    setOutputConnectionType((unsigned)Outputs::orderBefore, {.interpretation = ConnectionType::ConnectionType::DEPENDENCY, .width=0});
    m_clocks.resize(1);
}

void Node_MemPort::connectMemory(Node_Memory *memory)
{
    if (!memory->noConflicts()) {
        Node_MemPort *lastMemPort = memory->getLastPort();
        if (lastMemPort != nullptr)
            orderAfter(lastMemPort);
    }
    connectInput((unsigned)Inputs::memory, {.node=memory, .port=0});
}

void Node_MemPort::disconnectMemory()
{
    disconnectInput((unsigned)Inputs::memory);
}


Node_Memory *Node_MemPort::getMemory()
{
    return dynamic_cast<Node_Memory *>(getDriver((unsigned)Inputs::memory).node);
}

void Node_MemPort::connectEnable(const NodePort &output)
{
    connectInput((unsigned)Inputs::enable, output);
}

void Node_MemPort::connectWrEnable(const NodePort &output)
{
    HCL_ASSERT_HINT(!isReadPort(), "For now I don't want to mix read and write ports");
    connectInput((unsigned)Inputs::wrEnable, output);
}

void Node_MemPort::connectAddress(const NodePort &output)
{
    connectInput((unsigned)Inputs::address, output);
}

void Node_MemPort::connectWrData(const NodePort &output)
{
    HCL_ASSERT_HINT(!isReadPort(), "For now I don't want to mix read and write ports");
    connectInput((unsigned)Inputs::wrData, output);
}

void Node_MemPort::orderAfter(Node_MemPort *writePort)
{
    connectInput((unsigned)Inputs::orderAfter, {.node=writePort, .port=(unsigned)Node_MemPort::Outputs::orderBefore});
}

bool Node_MemPort::hasSideEffects() const
{
    return getDriver((unsigned)Inputs::memory).node != nullptr && getDriver((unsigned)Inputs::wrEnable).node != nullptr;
}


bool Node_MemPort::isOrderedAfter(Node_MemPort *port) const
{
    auto *p = (Node_MemPort *)getDriver((unsigned)Inputs::orderAfter).node;
    while (p != nullptr) {
        if (p == port) return true;
        p = (Node_MemPort *)p->getDriver((unsigned)Inputs::orderAfter).node;
    }
    return false;
}

bool Node_MemPort::isOrderedBefore(Node_MemPort *port) const
{
    auto *p = (Node_MemPort *)port->getDriver((unsigned)Inputs::orderAfter).node;
    while (p != nullptr) {
        if (p == this) return true;
        p = (Node_MemPort *)p->getDriver((unsigned)Inputs::orderAfter).node;
    }
    return false;
}


void Node_MemPort::setClock(Clock* clk)
{
    attachClock(clk, 0);
}

bool Node_MemPort::isReadPort()
{
    return !getDirectlyDriven((unsigned)Outputs::rdData).empty();
}

bool Node_MemPort::isWritePort()
{
    return getDriver((unsigned)Inputs::wrEnable).node != nullptr && getDriver((unsigned)Inputs::wrData).node != nullptr;
}



void Node_MemPort::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
}

void Node_MemPort::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
}



std::string Node_MemPort::getTypeName() const
{
    return "mem_port";
}

void Node_MemPort::assertValidity() const
{
}

std::string Node_MemPort::getInputName(size_t idx) const
{
    switch (idx) {
        case (unsigned)Inputs::memory:
            return "memory";
        case (unsigned)Inputs::address:
            return "addr";
        case (unsigned)Inputs::enable:
            return "enable";
        case (unsigned)Inputs::wrEnable:
            return "wrEnable";
        case (unsigned)Inputs::wrData:
            return "wrData";
        case (unsigned)Inputs::orderAfter:
            return "orderAfter";
        default:
            return "unknown";
    }
}

std::string Node_MemPort::getOutputName(size_t idx) const
{
    switch (idx) {
        case (unsigned)Outputs::rdData:
            return "rdData";
        case (unsigned)Outputs::orderBefore:
            return "orderBefore";
        default:
            return "unknown";
    }
}


std::vector<size_t> Node_MemPort::getInternalStateSizes() const
{
    return {};
}

}
