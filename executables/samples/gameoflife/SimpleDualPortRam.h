#pragma once

#include <metaHDL_core/frontend/Integers.h>
#include <metaHDL_core/frontend/BitVector.h>
#include <metaHDL_core/frontend/Bit.h>
#include <metaHDL_core/utils/Preprocessor.h>
#include <metaHDL_core/utils/StackTrace.h>
#include <metaHDL_core/utils/Exceptions.h>
#include <metaHDL_core/hlim/supportNodes/Node_External.h>
#include <metaHDL_core/simulation/BitVectorState.h>


using namespace mhdl::core::hlim;
using namespace mhdl::core;
using namespace mhdl::utils;
using namespace mhdl;


class SimpleDualPortRam : public Node_External
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
        enum Clock {
            WRITE_CLK,
            READ_CLK,
            NUM_CLOCKS
        };
                
        SimpleDualPortRam(BaseClock *writeClk, BaseClock *readClk, sim::DefaultBitVectorState initialData, size_t writeDataWidth=1, size_t readDataWidth=1, bool outputRegister=false);

        void connectInput(Input input, const NodePort &port);
        inline void disconnectInput(Input input) { NodeIO::disconnectInput(input); }
        
        bool isRom() const;
        
        virtual void simulateReset(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
        virtual void simulateEvaluate(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;        
        virtual void simulateAdvance(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets, size_t clockPort) const override;
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;
        virtual std::vector<size_t> getInternalStateSizes() const override;
    protected:
        sim::DefaultBitVectorState m_initialData;
        size_t m_writeDataWidth;
        size_t m_readDataWidth;
};

void buildDualPortRam(BaseClock *writeClk, BaseClock *readClk, size_t size, 
                      const frontend::Bit &writeEnable, const frontend::UnsignedInteger &writeAddress, const frontend::BitVector &writeData,
                      const frontend::Bit &readEnable, const frontend::UnsignedInteger &readAddress, frontend::BitVector &readData,
                      const frontend::BitVector &readDataResetValue);

void buildRom(BaseClock *clk, sim::DefaultBitVectorState data, 
                const frontend::Bit &readEnable, const frontend::UnsignedInteger &readAddress, frontend::BitVector &readData,
                const frontend::BitVector &readDataResetValue);
