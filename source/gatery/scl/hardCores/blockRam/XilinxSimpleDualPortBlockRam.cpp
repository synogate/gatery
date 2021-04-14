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
#include "gatery/pch.h"
#include "XilinxSimpleDualPortBlockRam.h"

#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/utils.h>

#include <boost/format.hpp>

#include <string_view>

namespace hcl::scl::blockram {

using namespace hlim;

/*
XilinxSimpleDualPortBlockRam::XilinxSimpleDualPortBlockRam(Clock *writeClk, Clock *readClk, sim::DefaultBitVectorState initialData, size_t writeDataWidth, size_t readDataWidth, bool outputRegister) :
            m_initialData(std::move(initialData))
{
    HCL_ASSERT_HINT(!outputRegister, "Read output register not yet implemented!");

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
        .interpretation = ConnectionType::BITVEC,
        .width = m_readDataWidth,
    });
    setOutputType(READ_DATA, OUTPUT_LATCHED);
}

void XilinxSimpleDualPortBlockRam::connectInput(Input input, const NodePort &port)
{
    switch (input) {
        case WRITE_ADDR:
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::BITVEC);
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == utils::Log2C(m_initialData.size() / m_writeDataWidth));
        break;
        case WRITE_DATA:
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::BITVEC);
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == m_writeDataWidth);
        break;
        case WRITE_ENABLE:
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::BOOL);
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == 1);
        break;
        case READ_ADDR:
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::BITVEC);
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == utils::Log2C(m_initialData.size() / m_readDataWidth));
        break;
        case READ_ENABLE:
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::BOOL);
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == 1);
        break;
        case RESET_READ_DATA:
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).interpretation == ConnectionType::BITVEC);
            HCL_DESIGNCHECK(port.node->getOutputConnectionType(port.port).width == m_readDataWidth);
        break;
        default:
            HCL_DESIGNCHECK_HINT(false, "Unknown input!");
    }
    NodeIO::connectInput(input, port);
}

bool XilinxSimpleDualPortBlockRam::isRom() const
{
    Node_Constant *cNode = dynamic_cast<Node_Constant*>(getNonSignalDriver(WRITE_ENABLE).node);
    return cNode != nullptr;
}



void XilinxSimpleDualPortBlockRam::simulateReset(SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
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
    HCL_ASSERT_HINT(constNode != nullptr, "Constant value propagation is not yet implemented, so for simulation the register reset value must be connected to a constant node via signals only!");

    state.insert(constNode->getValue(), outputOffsets[0]);
}


bool rangesOverlap(size_t range1_start, size_t range1_size, size_t range2_start, size_t range2_size)
{
    if (range1_start >= range2_start+range2_size)
        return false;
    if (range1_start+range1_size <= range2_start)
        return false;

    return true;
}


void XilinxSimpleDualPortBlockRam::simulateEvaluate(SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    NodePort drivers[NUM_INPUTS];
    for (auto i : utils::Range<size_t>(NUM_INPUTS))
        drivers[i] = getNonSignalDriver(i);


    HCL_ASSERT(drivers[WRITE_ENABLE].node != nullptr);

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
        HCL_ASSERT(drivers[WRITE_DATA].node != nullptr);
        HCL_ASSERT(drivers[WRITE_ADDR].node != nullptr);
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


    HCL_ASSERT(drivers[READ_ENABLE].node != nullptr);
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

        HCL_ASSERT(drivers[READ_ADDR].node != nullptr);

        if ((readAddress+m_readDataWidth > m_initialData.size()) ||
            ((!writeEnableDefined || writeEnable) && (!writeAddressDefined || rangesOverlap(readAddress, m_readDataWidth, writeAddress, m_writeDataWidth)))) {

            state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[INT_READ_DATA], m_readDataWidth);
        } else {
            state.copyRange(internalOffsets[INT_READ_DATA], state, internalOffsets[0] + readAddress, m_readDataWidth);
        }
    }

}

void XilinxSimpleDualPortBlockRam::simulateAdvance(SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
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

std::string XilinxSimpleDualPortBlockRam::getTypeName() const
{
    return "XilinxSimpleDualPortBlockRam";
}

void XilinxSimpleDualPortBlockRam::assertValidity() const
{

}

std::string XilinxSimpleDualPortBlockRam::getInputName(size_t idx) const
{
    switch (idx) {
        case WRITE_ADDR: return "WRITE_ADDR";
        case WRITE_DATA: return "WRITE_DATA";
        case WRITE_ENABLE: return "WRITE_ENABLE";
        case READ_ADDR: return "READ_ADDR";
        case READ_ENABLE: return "READ_ENABLE";
        case RESET_READ_DATA: return "RESET_READ_DATA";
    }
    return {};
}

std::string XilinxSimpleDualPortBlockRam::getOutputName(size_t idx) const
{
    switch (idx) {
        case READ_DATA: return "readData";
    }
    return {};
}

std::vector<size_t> XilinxSimpleDualPortBlockRam::getInternalStateSizes() const {

    std::vector<size_t> res(NUM_INTERNALS);
    res[INT_MEMORY] = m_initialData.size();
    res[INT_READ_DATA] = m_readDataWidth;
    res[INT_READ_ENABLE] = 1;
    return res;
}

bool XilinxSimpleDualPortBlockRam::writeVHDL(const vhdl::CodeFormatting *codeFormatting, std::ostream &file, const hlim::Node_External *node, unsigned indent,
                const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames, const std::vector<std::string> &clockNames)
{
    const XilinxSimpleDualPortBlockRam *ram = dynamic_cast<const XilinxSimpleDualPortBlockRam*>(node);
    if (ram != nullptr) {
        codeFormatting->indent(file, indent);
        file << "inst_" << node->getName() << " : BRAM_SDP_MACRO generic map (" << std::endl;

        std::vector<std::string> genericmapList;
        genericmapList.push_back("-- INIT => todo: evaluate const expression of input port");
        genericmapList.push_back("INIT => 0");
        genericmapList.push_back(std::string("WRITE_WIDTH => ") + std::to_string(ram->getWriteDataWidth()));
        genericmapList.push_back(std::string("READ_WIDTH => ") + std::to_string(ram->getReadDataWidth()));
        genericmapList.push_back(std::string("BRAM_SIZE => ") + std::to_string(ram->getInitialData().size()));
        if (ram->isRom())
            for (auto block : utils::Range((ram->getInitialData().size()+255)/256)) {
                std::stringstream hexStream;
                for (auto byteIdx : utils::Range(256/8)) {
                    size_t bitsRemaining = ram->getInitialData().size() - block*256 - byteIdx*8;
                    std::uint8_t byte = 0;
                    for (auto bitIdx : utils::Range(std::min<size_t>(8, bitsRemaining))) {
                        bool def = ram->getInitialData().get(sim::DefaultConfig::DEFINED, block*256 + byteIdx*8+bitIdx);
                        bool value = ram->getInitialData().get(sim::DefaultConfig::VALUE, block*256 + byteIdx*8+bitIdx);
                        ///@todo use def
                        if (value)
                            byte |= (1 << bitIdx);
                    }
                    hexStream << std::hex << std::setw(2) << std::setfill('0') << (unsigned) byte;
                }

                genericmapList.push_back((boost::format("INIT_%02d => X\"%s\"") % block % hexStream.str()).str());
            }

        for (auto i : utils::Range(genericmapList.size())) {
            codeFormatting->indent(file, indent+1);
            file << genericmapList[i];
            if (i+1 < genericmapList.size())
                file << ",";
            file << std::endl;
        }



        codeFormatting->indent(file, indent);
        file << ") port map (" << std::endl;

        std::vector<std::string> portmapList;

        if (!clockNames[XilinxSimpleDualPortBlockRam::READ_CLK].empty())
            portmapList.push_back(std::string("RDCLK => ") + clockNames[XilinxSimpleDualPortBlockRam::READ_CLK]);
        if (!clockNames[XilinxSimpleDualPortBlockRam::WRITE_CLK].empty())
            portmapList.push_back(std::string("WRCLK => ") + clockNames[XilinxSimpleDualPortBlockRam::WRITE_CLK]);
        portmapList.push_back("RST => reset");

        if (!inputSignalNames[XilinxSimpleDualPortBlockRam::READ_ENABLE].empty())
            portmapList.push_back(std::string("RDEN => ") + inputSignalNames[XilinxSimpleDualPortBlockRam::READ_ENABLE]);
        if (!inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_ENABLE].empty())
            portmapList.push_back(std::string("WREN => ") + inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_ENABLE]);
        if (!inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_DATA].empty())
            portmapList.push_back(std::string("DI => ") + inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_DATA]);
        if (!inputSignalNames[XilinxSimpleDualPortBlockRam::READ_ADDR].empty())
            portmapList.push_back(std::string("RDADDR => ") + inputSignalNames[XilinxSimpleDualPortBlockRam::READ_ADDR]);
        if (!inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_ADDR].empty())
            portmapList.push_back(std::string("WRADDR => ") + inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_ADDR]);

        if (!outputSignalNames[XilinxSimpleDualPortBlockRam::READ_DATA].empty())
            portmapList.push_back(std::string("DO => ") + outputSignalNames[XilinxSimpleDualPortBlockRam::READ_DATA]);

        for (auto i : utils::Range(portmapList.size())) {
            codeFormatting->indent(file, indent+1);
            file << portmapList[i];
            if (i+1 < portmapList.size())
                file << ",";
            file << std::endl;
        }

        codeFormatting->indent(file, indent);
        file << ");" << std::endl;

        return true;
    }
    return false;
}

bool XilinxSimpleDualPortBlockRam::writeIntelVHDL(const vhdl::CodeFormatting* codeFormatting, std::ostream& file, const hlim::Node_External* node, unsigned indent, const std::vector<std::string>& inputSignalNames, const std::vector<std::string>& outputSignalNames, const std::vector<std::string>& clockNames)
{
    const XilinxSimpleDualPortBlockRam* ram = dynamic_cast<const XilinxSimpleDualPortBlockRam*>(node);
    if (!ram)
        return false;
    HCL_ASSERT(ram->getWriteDataWidth() == ram->getReadDataWidth());

    codeFormatting->indent(file, indent);
    file << "inst_" << node->getName() << " : altdpram generic map (" << std::endl;

    bool firstPort = true;
    auto addPort = [&](std::string_view portName, std::string_view signalName) {
        if (!signalName.empty())
        {
            if (!firstPort) file << ",\n";
            firstPort = false;
            codeFormatting->indent(file, indent + 1);
            file << portName << " => " << signalName;
        }
    };


    addPort("lpm_type", "\"altdpram\"");
    addPort("read_during_write_mode_mixed_ports", "\"DONT_CARE\"");
    //addPort("width_byteena", "1");

    const size_t addrWidth = utils::Log2C(ram->getInitialData().size() / ram->getReadDataWidth());
    addPort("ram_block_type", addrWidth <= 6 ? "\"MLAB\"" : "\"AUTO\"");

    addPort("width", std::to_string(ram->getReadDataWidth()));
    addPort("widthad", std::to_string(addrWidth));

    // write data
    addPort("indata_aclr", "\"OFF\"");
    addPort("indata_reg", "\"INCLOCK\"");
    // write address
    addPort("wraddress_aclr", "\"OFF\"");
    addPort("wraddress_reg", "\"INCLOCK\"");
    // write ctrl
    addPort("wrcontrol_aclr", "\"OFF\"");
    addPort("wrcontrol_reg", "\"INCLOCK\"");
    // read data
    addPort("outdata_aclr", "\"OFF\"");
    addPort("outdata_reg", "\"UNREGISTERED\"");
    // read address
    addPort("rdaddress_aclr", "\"OFF\"");
    addPort("rdaddress_reg", "\"OUTCLOCK\"");
    // read ctrl
    addPort("rdcontrol_aclr", "\"OFF\"");
    addPort("rdcontrol_reg", "\"OUTCLOCK\"");

    file << '\n';
    codeFormatting->indent(file, indent);
    file << ") port map (" << std::endl;

    firstPort = true;

    addPort("outclock", clockNames[XilinxSimpleDualPortBlockRam::READ_CLK]);
    addPort("inclock", clockNames[XilinxSimpleDualPortBlockRam::WRITE_CLK]);

    addPort("rden", inputSignalNames[XilinxSimpleDualPortBlockRam::READ_ENABLE]);
    addPort("rdaddress", inputSignalNames[XilinxSimpleDualPortBlockRam::READ_ADDR]);
    addPort("q", outputSignalNames[XilinxSimpleDualPortBlockRam::READ_DATA]);

    addPort("wren", inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_ENABLE]);
    addPort("wraddress", inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_ADDR]);
    addPort("data", inputSignalNames[XilinxSimpleDualPortBlockRam::WRITE_DATA]);

    file << '\n';
    codeFormatting->indent(file, indent);
    file << ");" << std::endl;

    return true;
}

*/
}
