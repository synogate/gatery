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
#include "Node_Memory.h"

#include "Node_MemPort.h"

#include "../SignalDelay.h"

namespace gtry::hlim {

    Node_Memory::Node_Memory()
    {
        resizeInputs(1); // at least one initialization data port
        resizeOutputs((size_t)Outputs::COUNT);
        setOutputConnectionType((size_t)Outputs::INITIALIZATION_ADDR, {.interpretation = ConnectionType::BITVEC, .width=0});
        setOutputConnectionType((size_t)Outputs::READ_DEPENDENCIES, {.interpretation = ConnectionType::DEPENDENCY, .width=1});

        setType(MemType::DONT_CARE);
    }

    void Node_Memory::setInitializationNetDataWidth(size_t width)
    {
        HCL_ASSERT_HINT(getDirectlyDriven((size_t)Outputs::INITIALIZATION_ADDR).empty(), "Can't change memory initialization width when initialization network is already attached!");
        HCL_ASSERT_HINT(getDriver((size_t)Inputs::INITIALIZATION_DATA).node == nullptr, "Can't change memory initialization width when initialization network is already attached!");

        m_initializationDataWidth = width;
        if (width == 0ull) {
            setOutputConnectionType((size_t)Outputs::INITIALIZATION_ADDR, {.interpretation = ConnectionType::BITVEC, .width=0});
        } else {
            auto numAddresses = getSize() / width;
            setOutputConnectionType((size_t)Outputs::INITIALIZATION_ADDR, {.interpretation = ConnectionType::BITVEC, .width=utils::Log2C(numAddresses)});
        }
    }

    void Node_Memory::setType(MemType type, size_t requiredReadLatency)
    { 
        m_type = type; 
        m_requiredReadLatency = requiredReadLatency;
        switch (m_type) {
            case MemType::DONT_CARE:
                if (m_requiredReadLatency == ~0ull)
                    m_requiredReadLatency = 0;
            break;
            case MemType::LUTRAM:
                if (m_requiredReadLatency == ~0ull)
                    m_requiredReadLatency = 0;
            break;
            case MemType::BRAM:
                if (m_requiredReadLatency == ~0ull)
                    m_requiredReadLatency = 1;
            break;
            case MemType::EXTERNAL:
                if (m_requiredReadLatency == ~0ull)
                    m_requiredReadLatency = 1;
            break;
        }
    }

    void Node_Memory::setNoConflicts()
    {
        m_noConflicts = true;
        for (auto np : getPorts())
            if (auto *port = dynamic_cast<Node_MemPort*>(np.node))
                port->orderAfter(nullptr);
    }

    size_t Node_Memory::createWriteDependencyInputPort() 
    {
        resizeInputs(getNumInputPorts()+1);
        return getNumInputPorts()-1;
    }

    void Node_Memory::destroyWriteDependencyInputPort(size_t port) 
    {
        connectInput(port, getDriver(getNumInputPorts()-1));
        resizeInputs(getNumInputPorts()-1);
    }


    std::size_t Node_Memory::getMaxPortWidth() const {
        std::size_t size = 0;

        for (auto np : getPorts()) {
            auto *port = dynamic_cast<Node_MemPort*>(np.node);
            size = std::max(size, port->getBitWidth());
        }

        return size;
    }

    void Node_Memory::setPowerOnState(sim::DefaultBitVectorState powerOnState)
    {
        m_powerOnState = std::move(powerOnState);
        setInitializationNetDataWidth(m_initializationDataWidth);
    }

    void  Node_Memory::fillPowerOnState(sim::DefaultBitVectorState powerOnState) 
    {
        HCL_DESIGNCHECK_HINT(powerOnState.size() <= m_powerOnState.size(), "Power-on state does not fit into memory!");
        if (powerOnState.size() == m_powerOnState.size())
            m_powerOnState = std::move(powerOnState);
        else {
            m_powerOnState.copyRange(0, powerOnState, 0, powerOnState.size());
            m_powerOnState.clearRange(sim::DefaultConfig::DEFINED, powerOnState.size(), m_powerOnState.size()-powerOnState.size());
        }
    }


    void Node_Memory::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
    {
        state.copyRange(internalOffsets[0], m_powerOnState, 0, m_powerOnState.size());
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[(size_t)Outputs::READ_DEPENDENCIES], 1);
    }

    void Node_Memory::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
    {
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[(size_t)Outputs::READ_DEPENDENCIES], 1);
    }

    bool Node_Memory::isROM() const {
        for (auto np : getPorts())
            if (auto *port = dynamic_cast<Node_MemPort*>(np.node))
                if (port->isWritePort()) return false;

        return true;
    }

    Node_MemPort *Node_Memory::getLastPort()
    {
        for (auto np : getPorts()) {
            if (auto *port = dynamic_cast<Node_MemPort*>(np.node)) {
                if (port->getDirectlyDriven((unsigned)Node_MemPort::Outputs::orderBefore).empty())
                    return port;
            }
        }
        return nullptr;
    }

    std::string Node_Memory::getTypeName() const
    {
        return "memory";
    }

    void Node_Memory::assertValidity() const
    {
    }

    std::string Node_Memory::getInputName(size_t idx) const
    {
        if (idx == 0)
            return "INITIALIZATION_DATA";
        return "WRITE_DEPENDENCY";
    }

    std::string Node_Memory::getOutputName(size_t idx) const
    {
        if (idx == 0)
            return "INITIALIZATION_ADDR";
        return "READ_DEPENDENCY";
    }

    std::vector<size_t> Node_Memory::getInternalStateSizes() const
    {
        return { m_powerOnState.size() };
    }





    std::unique_ptr<BaseNode> Node_Memory::cloneUnconnected() const
    {
        std::unique_ptr<BaseNode> res(new Node_Memory());
        copyBaseToClone(res.get());
        ((Node_Memory*)res.get())->m_powerOnState = m_powerOnState;
        ((Node_Memory*)res.get())->m_type = m_type;
        ((Node_Memory*)res.get())->m_noConflicts = m_noConflicts;
        return res;
    }

    void Node_Memory::estimateSignalDelay(SignalDelay &sigDelay)
    {
        /*
        HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
        auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});
        for (auto &f : outDelay)
            f = 0.0f; 
        */
    }

}
