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

#include <gatery/hlim/Clock.h>
#include <gatery/hlim/supportNodes/Node_External.h>
#include <gatery/export/vhdl/CodeFormatting.h>
#include <gatery/simulation/BitVectorState.h>

#include <vector>

namespace gtry::scl::blockram {
/*
class XilinxSimpleDualPortBlockRam : public hlim::Node_External
{
    public:
        enum Input {
            WRITE_ADDR,
            WRITE_DATA,
            WRITE_ENABLE,
            READ_ADDR,
            READ_ENABLE,
            RESET_READ_DATA,
            NUM_INPUTS
        };
        enum Output {
            READ_DATA,
            NUM_OUTPUTS
        };
        enum Internal {
            INT_MEMORY,
            INT_READ_DATA,
            INT_READ_ENABLE,
            NUM_INTERNALS
        };
        enum Clocks {
            WRITE_CLK,
            READ_CLK,
            NUM_CLOCKS
        };

        using Clock = gtry::hlim::Clock;
        using NodePort = gtry::hlim::NodePort;
        using DefaultBitVectorState = gtry::sim::DefaultBitVectorState;
        using SimulatorCallbacks = gtry::sim::SimulatorCallbacks;

        XilinxSimpleDualPortBlockRam(Clock *writeClk, Clock *readClk, DefaultBitVectorState initialData,
                                size_t writeDataWidth=1, size_t readDataWidth=1, bool outputRegister=false);

        void connectInput(Input input, const NodePort &port);
        inline void disconnectInput(Input input) { NodeIO::disconnectInput(input); }

        bool isRom() const;

        virtual void simulateReset(SimulatorCallbacks &simCallbacks, DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
        virtual void simulateEvaluate(SimulatorCallbacks &simCallbacks, DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;
        virtual void simulateAdvance(SimulatorCallbacks &simCallbacks, DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const override;

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;
        virtual std::vector<size_t> getInternalStateSizes() const override;

        inline const DefaultBitVectorState &getInitialData() const { return m_initialData; }
        inline size_t getWriteDataWidth() const { return m_writeDataWidth; }
        inline size_t getReadDataWidth() const { return m_readDataWidth; }


        static bool writeVHDL(const vhdl::CodeFormatting *codeFormatting, std::ostream &file, const hlim::Node_External *node, unsigned indent,
                        const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames, const std::vector<std::string> &clockNames);
        static bool writeIntelVHDL(const vhdl::CodeFormatting* codeFormatting, std::ostream& file, const hlim::Node_External* node, unsigned indent,
            const std::vector<std::string>& inputSignalNames, const std::vector<std::string>& outputSignalNames, const std::vector<std::string>& clockNames);
protected:
        DefaultBitVectorState m_initialData;
        size_t m_writeDataWidth;
        size_t m_readDataWidth;
};
*/
}
