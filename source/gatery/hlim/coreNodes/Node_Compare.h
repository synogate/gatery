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

class Node_Compare : public Node<Node_Compare>
{
    public:
        enum Op {
            EQ,
            NEQ,
            LT,
            GT,
            LEQ,
            GEQ
        };

        Node_Compare(Op op);
        inline Op getOp() const { return m_op; }

        inline void connectInput(size_t operand, const NodePort &port) { NodeIO::connectInput(operand, port); }
        inline void disconnectInput(size_t operand) { NodeIO::disconnectInput(operand); }

        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual std::string attemptInferOutputName(size_t outputPort) const;

        virtual void estimateSignalDelay(SignalDelay &sigDelay) override;

        virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, unsigned outputPort, unsigned outputBit, unsigned &inputPort, unsigned &inputBit) override;
    protected:
        Op m_op;
};

}
