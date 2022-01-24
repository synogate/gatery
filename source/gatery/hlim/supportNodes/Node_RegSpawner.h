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

/**
 * @brief Spawns registers for retiming
 * @details Defines a source of infinite registers that the forward retiming can pull from.
 * Registers always spawn for all signals going through to spawner, effectively keeping the signals in sync.
 */
class Node_RegSpawner : public Node<Node_RegSpawner>
{
    public:
        Node_RegSpawner();

        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

        size_t addInput(const NodePort &value, const NodePort &reset = {});
        void setClock(Clock* clk);

        void bypass();
		void spawnForward();

        virtual bool hasSideEffects() const override { return false; }
		virtual bool isCombinatorial() const { return true; }

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual void estimateSignalDelay(SignalDelay &sigDelay) override;

        virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit) override;

		inline size_t getNumStagesSpawned() const { return m_numStagesSpawned; }
    protected:
		size_t m_numStagesSpawned = 0ull;
};

}
