#pragma once

#include <hcl/hlim/Clock.h>
#include <hcl/hlim/supportNodes/Node_External.h>
#include <hcl/export/vhdl/CodeFormatting.h>
#include <hcl/simulation/BitVectorState.h>

#include <vector>

namespace hcl::stl::blockram {
/*
class XilinxSimpleDualPortBlockRam : public core::hlim::Node_External
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

        using Clock = hcl::core::hlim::Clock;
        using NodePort = hcl::core::hlim::NodePort;
        using DefaultBitVectorState = hcl::core::sim::DefaultBitVectorState;
        using SimulatorCallbacks = hcl::core::sim::SimulatorCallbacks;

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


        static bool writeVHDL(const core::vhdl::CodeFormatting *codeFormatting, std::ostream &file, const core::hlim::Node_External *node, unsigned indent,
                        const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames, const std::vector<std::string> &clockNames);
        static bool writeIntelVHDL(const core::vhdl::CodeFormatting* codeFormatting, std::ostream& file, const core::hlim::Node_External* node, unsigned indent,
            const std::vector<std::string>& inputSignalNames, const std::vector<std::string>& outputSignalNames, const std::vector<std::string>& clockNames);
protected:
        DefaultBitVectorState m_initialData;
        size_t m_writeDataWidth;
        size_t m_readDataWidth;
};
*/
}
