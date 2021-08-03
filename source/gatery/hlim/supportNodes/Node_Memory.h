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
#pragma once

#include "../Node.h"


namespace gtry::hlim {

class Node_MemPort;


/**
 * @brief Abstract memory node to represent e.g. BlockRams or LutRams
 *
 *
 * The Node_Memory can be accessed by connecting Node_MemReadPort and Node_MemWritePort as read and write ports.
 * Arbitrarily many read and write ports can be connected.
 * The Node_Memory serves as the entity that represents the stored information and groups the read and write ports together.
 * Writing through a write port is always synchronous with a one clock delay, reading is asynchronous but can be made synchronous
 * by adding registers to the data output.
 *
 * The connections are made through 1-bit wide connections between the nodes of type "ConnectionType::DEPENDENCY".
 *
 */

class Node_Memory : public Node<Node_Memory>
{
    public:
        enum class MemType {
            DONT_CARE,
            LUTRAM,
            BRAM,
        };

        enum class Internal {
            data,
            count
        };

        Node_Memory();

        void setType(MemType type) { m_type = type; }
        void setNoConflicts();

        std::size_t getSize() const { return m_powerOnState.size(); }
        std::size_t getMaxPortWidth() const;
        void setPowerOnState(sim::DefaultBitVectorState powerOnState);

        /// Overwrites the head of the power on state without resizing, rest is undefined
        void fillPowerOnState(sim::DefaultBitVectorState powerOnState);


        const sim::DefaultBitVectorState &getPowerOnState() const { return m_powerOnState; }
        sim::DefaultBitVectorState &getPowerOnState() { return m_powerOnState; }

        virtual void simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

        virtual bool hasSideEffects() const override { return hasRef(); } // for now

        bool isROM() const;

        Node_MemPort *getLastPort();

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;

        MemType type() const { return m_type; }
        bool noConflicts() const { return m_noConflicts; }

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual void estimateSignalDelay(SignalDelay &sigDelay) override;
        //virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit) override;
    protected:
        sim::DefaultBitVectorState m_powerOnState;

        MemType m_type = MemType::DONT_CARE;
        bool m_noConflicts = false;
};

}
