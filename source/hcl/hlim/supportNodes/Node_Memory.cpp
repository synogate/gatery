/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "Node_Memory.h"

#include "Node_MemPort.h"

namespace hcl::core::hlim {

    Node_Memory::Node_Memory()
    {
        resizeOutputs(1);
        setOutputConnectionType(0, {.interpretation = ConnectionType::DEPENDENCY, .width=1});
    }

    void Node_Memory::setNoConflicts()
    {
        m_noConflicts = true;
        for (auto np : getDirectlyDriven(0))
            if (auto *port = dynamic_cast<Node_MemPort*>(np.node))
                port->orderAfter(nullptr);
    }

    std::size_t Node_Memory::getMaxPortWidth() const {
        std::size_t size = 0;

        for (auto np : getDirectlyDriven(0)) {
            auto *port = dynamic_cast<Node_MemPort*>(np.node);
            size = std::max(size, port->getBitWidth());
        }

        return size;
    }

    void Node_Memory::setPowerOnState(sim::DefaultBitVectorState powerOnState)
    {
        m_powerOnState = std::move(powerOnState);
    }

    void Node_Memory::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
    {
        state.copyRange(internalOffsets[0], m_powerOnState, 0, m_powerOnState.size());
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], 1);
    }

    void Node_Memory::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
    {
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], 1);
    }

    bool Node_Memory::isROM() const {
        for (auto np : getDirectlyDriven(0))
            if (auto *port = dynamic_cast<Node_MemPort*>(np.node))
                if (port->isWritePort()) return false;

        return true;
    }

    Node_MemPort *Node_Memory::getLastPort()
    {
        for (auto np : getDirectlyDriven(0)) {
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
        return "";
    }

    std::string Node_Memory::getOutputName(size_t idx) const
    {
        return "memory_ports";
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


}
