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
#include "gatery/pch.h"
#include "Node_MemPort.h"

#include "Node_Memory.h"

namespace gtry::hlim {

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


Node_Memory *Node_MemPort::getMemory() const
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
    return getDriver((unsigned)Inputs::memory).node != nullptr && getDriver((unsigned)Inputs::wrData).node != nullptr;
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

bool Node_MemPort::isReadPort() const
{
    return !getDirectlyDriven((unsigned)Outputs::rdData).empty();
}

bool Node_MemPort::isWritePort() const
{
    return getDriver((unsigned)Inputs::wrData).node != nullptr;
}



void Node_MemPort::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
    // clean all internal stuff
    if (isWritePort()) {
        state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[(unsigned)Internal::address], getDriverConnType((unsigned)Inputs::address).width);
        state.clearRange(sim::DefaultConfig::DEFINED,internalOffsets[(unsigned)Internal::wrData], getBitWidth());
        state.clear(sim::DefaultConfig::VALUE, internalOffsets[(unsigned)Internal::wrEnable]);
    }
}

void Node_MemPort::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    const auto &addrType = getDriverConnType((unsigned)Inputs::address);

    // optional enables, defaults to true
    bool enableValue = true;
    bool enableDefined = true;
    if (inputOffsets[(unsigned)Inputs::enable] != ~0ull) {
        enableValue   = state.get(sim::DefaultConfig::VALUE, inputOffsets[(unsigned)Inputs::enable]);
        enableDefined = state.get(sim::DefaultConfig::DEFINED, inputOffsets[(unsigned)Inputs::enable]);
    }

    // First, do an asynchronous read.
    if (outputOffsets[(unsigned)Outputs::rdData] != 0ull) {

        // If not enabled (or undefined) output undefined since this is an asynchronous read. For synchronous (BRAM) behavior,
        // the register after the read ports holds the read value on a disabled read.
        if (!enableValue || !enableDefined) {
            state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[(unsigned)Outputs::rdData], getBitWidth());
        } else {
            // Output is undefined if any address bits are undefined.
            // Otherwise the data is read from memory with a (not necessarily pow2) wrap around of the
            // address is larger than the memory.

            std::uint64_t addressValue   = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[(unsigned)Inputs::address], addrType.width);
            std::uint64_t addressDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[(unsigned)Inputs::address], addrType.width);

            if (addressDefined != (~0ull >> (64 - addrType.width))) {
                state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[(unsigned)Outputs::rdData], getBitWidth());
            } else {
                auto memSize = getMemory()->getSize();
                HCL_ASSERT(memSize % getBitWidth() == 0);
                auto index = (addressValue * getBitWidth()) % memSize;
                state.copyRange(outputOffsets[(unsigned)Outputs::rdData], state, internalOffsets[(unsigned)RefInternal::memory] + index, getBitWidth());
            }
        }
    }

    // If this is also a write port, store all information needed for the write in internal state
    // to perform the write on the clock edge.
    if (isWritePort()) {
        state.copyRange(internalOffsets[(unsigned)Internal::address], state, inputOffsets[(unsigned)Inputs::address], addrType.width);

        const auto &wrDataType = getDriverConnType((unsigned)Inputs::wrData);
        state.copyRange(internalOffsets[(unsigned)Internal::wrData], state, inputOffsets[(unsigned)Inputs::wrData], wrDataType.width);

        bool doWrite = enableValue || !enableDefined;
        // optional enables, defaults to true
        if (inputOffsets[(unsigned)Inputs::wrEnable] != ~0ull) {
            doWrite &= state.get(sim::DefaultConfig::VALUE, inputOffsets[(unsigned)Inputs::wrEnable]) ||
                        !state.get(sim::DefaultConfig::DEFINED, inputOffsets[(unsigned)Inputs::wrEnable]);
        }
        state.set(sim::DefaultConfig::VALUE, internalOffsets[(unsigned)Internal::wrEnable], doWrite);
    }
}

void Node_MemPort::simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
{
    if (isWritePort()) {
        bool doWrite = state.get(sim::DefaultConfig::VALUE, internalOffsets[(unsigned)Internal::wrEnable]);

        const auto &addrType = getDriverConnType((unsigned)Inputs::address);
        std::uint64_t addressValue   = state.extractNonStraddling(sim::DefaultConfig::VALUE, internalOffsets[(unsigned)Internal::address], addrType.width);
        std::uint64_t addressDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, internalOffsets[(unsigned)Internal::address], addrType.width);

        if (doWrite) {
            if (addressDefined != (~0ull >> (64 - addrType.width))) {
                // If the address is undefined, make the entire RAM undefined
                state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[(unsigned)RefInternal::memory], getMemory()->getSize());
            } else {
                // Perform write, same index computation/behavior as for reads
                auto memSize = getMemory()->getSize();
                HCL_ASSERT(memSize % getBitWidth() == 0);
                auto index = (addressValue * getBitWidth()) % memSize;
                state.copyRange(internalOffsets[(unsigned)RefInternal::memory] + index, state, internalOffsets[(unsigned)Internal::wrData], getBitWidth());
            }
        }
    }
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
    std::vector<size_t> sizes((unsigned)Internal::count, 0);
    if (isWritePort()) {

        if (auto driver = getDriver((unsigned)Inputs::wrData); driver.node != nullptr)
            sizes[(unsigned)Internal::wrData] = driver.node->getOutputConnectionType(driver.port).width;

        sizes[(unsigned)Internal::wrEnable] = 1;

        if (auto driver = getDriver((unsigned)Inputs::address); driver.node != nullptr)
            sizes[(unsigned)Internal::address] = driver.node->getOutputConnectionType(driver.port).width;
    }

    return sizes;
}

std::vector<std::pair<BaseNode*, size_t>> Node_MemPort::getReferencedInternalStateSizes() const
{
    return {{getMemory(), (unsigned)Node_Memory::Internal::data}};
}

std::unique_ptr<BaseNode> Node_MemPort::cloneUnconnected() const
{
    std::unique_ptr<BaseNode> res(new Node_MemPort(m_bitWidth));
    copyBaseToClone(res.get());
    return res;
}




}
