#include "SimpleDualPortRam.h"

#include <metaHDL_core/utils/BitManipulation.h>
#include <metaHDL_core/hlim/coreNodes/Node_Constant.h>
#include <metaHDL_core/frontend/Scope.h>
#include <metaHDL_core/frontend/Constant.h>

#include <iostream>

using namespace mhdl::core::frontend;


SimpleDualPortRam::SimpleDualPortRam(BaseClock *writeClk, BaseClock *readClk, sim::DefaultBitVectorState initialData, size_t writeDataWidth, size_t readDataWidth, bool outputRegister) : 
            m_initialData(std::move(initialData))
{    
    MHDL_ASSERT_HINT(!outputRegister, "Read output register not yet implemented!");
    
    m_writeDataWidth = writeDataWidth;
    m_readDataWidth = readDataWidth;
    m_clocks.resize(NUM_CLOCKS);
    if (writeClk != nullptr)
        attachClock(writeClk, WRITE_CLK);
    if (readClk != nullptr)
        attachClock(readClk, READ_CLK);

    resizeInputs(NUM_INPUTS);

    resizeOutputs(NUM_OUTPUTS);
    setOutputConnectionType(READ_DATA, {
        .interpretation = ConnectionType::RAW,
        .width = m_readDataWidth,
    });
    setOutputType(READ_DATA, OUTPUT_LATCHED);
}

void SimpleDualPortRam::connectInput(Input input, const NodePort &port)
{
    switch (input) {
        case WRITE_ADDR:
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::UNSIGNED);
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == utils::truncLog2(utils::nextPow2(m_initialData.size() / m_writeDataWidth)));
        break;
        case WRITE_DATA:
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::RAW);
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == m_writeDataWidth);
        break;
        case WRITE_ENABLE:
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::BOOL);
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == 1);
        break;
        case READ_ADDR:
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::UNSIGNED);
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == utils::truncLog2(utils::nextPow2(m_initialData.size() / m_readDataWidth)));
        break;
        case READ_ENABLE:
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::BOOL);
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == 1);
        break;
        case RESET_READ_DATA:
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::RAW);
            MHDL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == m_readDataWidth);
        break;
        default:
            MHDL_DESIGNCHECK_HINT(false, "Unknown input!");
    }
    NodeIO::connectInput(input, port); 
}

bool SimpleDualPortRam::isRom() const
{
    Node_Constant *cNode = dynamic_cast<Node_Constant*>(getNonSignalDriver(WRITE_ENABLE).node);    
    return cNode != nullptr && !cNode->getValue().bitVec[0];
}


        
void SimpleDualPortRam::simulateReset(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
    if (isRom()) {
        state.copyRange(internalOffsets[0], m_initialData, 0, m_initialData.size());
    } else {
        state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[0], m_initialData.size());
    }
    
    auto resetDriver = getNonSignalDriver(RESET_READ_DATA);
    if (resetDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[READ_DATA], getOutputConnectionType(READ_DATA).width, false);
        return;
    }

    Node_Constant *constNode = dynamic_cast<Node_Constant *>(resetDriver.node);
    MHDL_ASSERT_HINT(constNode != nullptr, "Constant value propagation is not yet implemented, so for simulation the register reset value must be connected to a constant node via signals only!");
    
    size_t width = getOutputConnectionType(0).width;
    size_t offset = 0;
    
    while (offset < width) {
        size_t chunkSize = std::min<size_t>(64, width-offset);
        
        std::uint64_t block = 0;
        for (auto i : utils::Range(chunkSize))
            if (constNode->getValue().bitVec[offset + i])
                block |= 1 << i;
                    
        state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[READ_DATA] + offset, chunkSize, block);
        state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[READ_DATA] + offset, chunkSize, ~0ull);    
        
        offset += chunkSize;
    }

}


bool rangesOverlap(size_t range1_start, size_t range1_size, size_t range2_start, size_t range2_size)
{
    if (range1_start >= range2_start+range2_size)
        return false;
    if (range1_start+range1_size <= range2_start)
        return false;
    
    return true;
}


void SimpleDualPortRam::simulateEvaluate(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    NodePort drivers[NUM_INPUTS];
    for (auto i : utils::Range<size_t>(NUM_INPUTS)) 
        drivers[i] = getNonSignalDriver(i);

    
    MHDL_ASSERT(drivers[WRITE_ENABLE].node != nullptr);
        
    bool writeEnableDefined = state.get(sim::DefaultConfig::DEFINED, inputOffsets[WRITE_ENABLE]);
    bool writeEnable = state.get(sim::DefaultConfig::VALUE, inputOffsets[WRITE_ENABLE]);
    
    size_t writeAddrWidth = 0; 
    bool writeAddressDefined = false;
    size_t writeAddress = ~0u;
    if (drivers[WRITE_ADDR].node != nullptr) {
        writeAddrWidth = drivers[WRITE_ADDR].node->getOutputConnectionType(drivers[WRITE_ADDR].port).width;        
        writeAddressDefined = allDefinedNonStraddling(state, inputOffsets[WRITE_ADDR], writeAddrWidth);
        writeAddress = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[WRITE_ADDR], writeAddrWidth) * m_writeDataWidth;
    }

    if (!writeEnableDefined || writeEnable) {
        MHDL_ASSERT(drivers[WRITE_DATA].node != nullptr);
        MHDL_ASSERT(drivers[WRITE_ADDR].node != nullptr);
    }
        
        
    if (!writeEnableDefined) {
        if (!writeAddressDefined || (writeAddress+m_writeDataWidth > m_initialData.size())) {
            state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[0], m_initialData.size());
        } else {
            state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[0] + writeAddress, m_writeDataWidth);
        }
    } else {
        if (writeEnable) {
            if (!writeAddressDefined || (writeAddress+m_writeDataWidth > m_initialData.size())) {
                state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[0], m_initialData.size());
            } else {
                state.copyRange(internalOffsets[0] + writeAddress, state, inputOffsets[WRITE_DATA], m_writeDataWidth);
            }
        }
    }

    
    MHDL_ASSERT(drivers[READ_ENABLE].node != nullptr);
    state.copyRange(internalOffsets[INT_READ_ENABLE], state, inputOffsets[READ_ENABLE], 1);
    
    bool readEnableDefined = state.get(sim::DefaultConfig::DEFINED, inputOffsets[READ_ENABLE]);
    bool readEnable = state.get(sim::DefaultConfig::VALUE, inputOffsets[READ_ENABLE]);
    
    if (readEnableDefined && readEnable)  {
        size_t readAddrWidth = 0; 
        bool readAddressDefined = false;
        size_t readAddress = ~0u;
        if (drivers[READ_ADDR].node != nullptr) {
            readAddrWidth = drivers[READ_ADDR].node->getOutputConnectionType(drivers[READ_ADDR].port).width;        
            readAddressDefined = allDefinedNonStraddling(state, inputOffsets[READ_ADDR], readAddrWidth);
            readAddress = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[READ_ADDR], readAddrWidth) * m_readDataWidth;
        }

        MHDL_ASSERT(drivers[READ_ADDR].node != nullptr);
        
        if ((readAddress+m_readDataWidth > m_initialData.size()) ||
            ((!writeEnableDefined || writeEnable) && (!writeAddressDefined || rangesOverlap(readAddress, m_readDataWidth, writeAddress, m_writeDataWidth)))) {
            
            state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[INT_READ_DATA], m_readDataWidth);
        } else {
            state.copyRange(internalOffsets[INT_READ_DATA], state, internalOffsets[0] + readAddress, m_readDataWidth);
        }
    }

}

void SimpleDualPortRam::simulateAdvance(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
{
    if (clockPort == READ_CLK) {
        bool readEnableDefined = state.get(sim::DefaultConfig::DEFINED, internalOffsets[INT_READ_ENABLE]);
        bool readEnable = state.get(sim::DefaultConfig::VALUE, internalOffsets[INT_READ_ENABLE]);
        
        if (!readEnableDefined)
            state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[READ_DATA], m_readDataWidth);
        else
            if (readEnable)
                state.copyRange(outputOffsets[READ_DATA], state, internalOffsets[INT_READ_DATA], m_readDataWidth);
    }
}
    
std::string SimpleDualPortRam::getTypeName() const 
{ 
    return "SimpleDualPortRam"; 
}

void SimpleDualPortRam::assertValidity() const 
{ 
    
}

std::string SimpleDualPortRam::getInputName(size_t idx) const 
{
    switch (idx) {
        case WRITE_ADDR: return "WRITE_ADDR";
        case WRITE_DATA: return "WRITE_DATA";
        case WRITE_ENABLE: return "WRITE_ENABLE";
        case READ_ADDR: return "READ_ADDR";
        case READ_ENABLE: return "READ_ENABLE";
        case RESET_READ_DATA: return "RESET_READ_DATA";
    }
}

std::string SimpleDualPortRam::getOutputName(size_t idx) const 
{
    switch (idx) {
        case READ_DATA: return "readData";
    }
}

std::vector<size_t> SimpleDualPortRam::getInternalStateSizes() const {
    
    std::vector<size_t> res(NUM_INTERNALS);
    res[INT_MEMORY] = m_initialData.size();
    res[INT_READ_DATA] = m_readDataWidth;
    res[INT_READ_ENABLE] = 1;
    return res;
}

        
        
void buildDualPortRam(BaseClock *writeClk, BaseClock *readClk, size_t size, 
                      const frontend::Bit &writeEnable, const frontend::UnsignedInteger &writeAddress, const frontend::BitVector &writeData,
                      const frontend::Bit &readEnable, const frontend::UnsignedInteger &readAddress, frontend::BitVector &readData,
                      const frontend::BitVector &readDataResetValue)
{
    sim::DefaultBitVectorState dummyData;
    dummyData.resize(size);
    SimpleDualPortRam *dbram = frontend::DesignScope::createNode<SimpleDualPortRam>(writeClk, readClk, std::move(dummyData), writeData.getWidth(), readData.getWidth());
    dbram->recordStackTrace();
    dbram->connectInput(SimpleDualPortRam::WRITE_ADDR,      {.node = writeAddress.getNode(), .port = 0});
    dbram->connectInput(SimpleDualPortRam::WRITE_DATA,      {.node = writeData.getNode(), .port = 0});
    dbram->connectInput(SimpleDualPortRam::WRITE_ENABLE,    {.node = writeEnable.getNode(), .port = 0});
    dbram->connectInput(SimpleDualPortRam::READ_ADDR,       {.node = readAddress.getNode(), .port = 0});
    dbram->connectInput(SimpleDualPortRam::READ_ENABLE,     {.node = readEnable.getNode(), .port = 0});
    dbram->connectInput(SimpleDualPortRam::RESET_READ_DATA, {.node = readDataResetValue.getNode(), .port = 0});
    
    readData = frontend::BitVector({.node = dbram, .port = SimpleDualPortRam::READ_DATA});
}

void buildRom(BaseClock *clk, sim::DefaultBitVectorState data, 
                const frontend::Bit &readEnable, const frontend::UnsignedInteger &readAddress, frontend::BitVector &readData,
                const frontend::BitVector &readDataResetValue)
{
    frontend::Bit writeEnable = 0_bit;
    
    SimpleDualPortRam *dbram = frontend::DesignScope::createNode<SimpleDualPortRam>(nullptr, clk, std::move(data), 1, readData.getWidth());
    dbram->recordStackTrace();
    dbram->connectInput(SimpleDualPortRam::WRITE_ENABLE,    {.node = writeEnable.getNode(), .port = 0});
    dbram->connectInput(SimpleDualPortRam::READ_ADDR,       {.node = readAddress.getNode(), .port = 0});
    dbram->connectInput(SimpleDualPortRam::READ_ENABLE,     {.node = readEnable.getNode(), .port = 0});
    dbram->connectInput(SimpleDualPortRam::RESET_READ_DATA, {.node = readDataResetValue.getNode(), .port = 0});
    
    readData = frontend::BitVector({.node = dbram, .port = SimpleDualPortRam::READ_DATA});
}

