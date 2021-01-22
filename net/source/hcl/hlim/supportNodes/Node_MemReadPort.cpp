#include "Node_MemReadPort.h"

#include "Node_Memory.h"

namespace hcl::core::hlim {

Node_MemReadPort::Node_MemReadPort(std::size_t bitWidth) : m_bitWidth(bitWidth)
{
    resizeInputs((unsigned)Inputs::count);
    resizeOutputs((unsigned)Outputs::count);
    setOutputConnectionType((unsigned)Outputs::data, {.interpretation = ConnectionType::BITVEC, .width=bitWidth});
}

Node_Memory *Node_MemReadPort::getMemory()
{
    return dynamic_cast<Node_Memory *>(getDriver((unsigned)Inputs::memory).node);
}

NodePort Node_MemReadPort::getDataPort()
{
    return {.node = this, .port = (unsigned)Outputs::data};
}

void Node_MemReadPort::connectEnable(const NodePort &output)
{
    connectInput((unsigned)Inputs::enable, output);
}

void Node_MemReadPort::connectAddress(const NodePort &output)
{
    connectInput((unsigned)Inputs::address, output);
}

std::string Node_MemReadPort::getTypeName() const
{
    return "mem_read_port";
}

void Node_MemReadPort::assertValidity() const
{
}

std::string Node_MemReadPort::getInputName(size_t idx) const
{
    switch (idx) {
        case (unsigned)Inputs::address:
            return "addr";
        case (unsigned)Inputs::enable:
            return "enable";
        case (unsigned)Inputs::memory:
            return "memory";
        default:
            return "unknown";
    }
}

std::string Node_MemReadPort::getOutputName(size_t idx) const
{
    return "data";
}


std::vector<size_t> Node_MemReadPort::getInternalStateSizes() const
{
    return {};
}


void Node_MemReadPort::connectMemory(Node_Memory *memory)
{
    connectInput((unsigned)Inputs::memory, {.node=memory, .port=0});
}

void Node_MemReadPort::disconnectMemory()
{
    disconnectInput((unsigned)Inputs::memory);
}


}
