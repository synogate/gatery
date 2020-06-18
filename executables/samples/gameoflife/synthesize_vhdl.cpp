#include "gameoflife.h"
#include "SimpleDualPortRam.h"

#include <metaHDL_core/utils/Range.h>

#include <boost/format.hpp>

using namespace mhdl::core;
using namespace mhdl;

int main()
{
    DesignScope design;

#if 0    
    {
        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all");

        auto clk = design.createClock<mhdl::core::hlim::RootClock>("clk", mhdl::core::hlim::ClockRational(10'000));
        RegisterConfig regConf = {.clk = clk, .resetName = "rst"};
        RegisterFactory registerFactory(regConf);

        GameOfLife game(32);

        BitStream in;
        MHDL_NAMED(in.data);
        MHDL_NAMED(in.valid);

        BitStream out = game(registerFactory, in);
    }
#else
    {
        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all");

        auto clk = design.createClock<mhdl::core::hlim::RootClock>("clk", mhdl::core::hlim::ClockRational(10'000));
        RegisterConfig regConf = {.clk = clk, .resetName = "rst"};
        RegisterFactory registerFactory(regConf);
        

        frontend::Bit ramWriteEnable;
        frontend::UnsignedInteger ramWriteAddress(10);
        frontend::BitVector ramWriteData(1);

        frontend::Bit ramReadEnable;
        frontend::UnsignedInteger ramReadAddress(10);
        frontend::BitVector ramReadData(1);

        
        frontend::Bit initialized;
        MHDL_NAMED(initialized);
        {
            GroupScope entity(NodeGroup::GRP_ENTITY);
            entity.setName("initializer");

            GroupScope area(NodeGroup::GRP_AREA);
            area.setName("all"); // ???

            sim::DefaultBitVectorState initialFrameBuffer;
            initialFrameBuffer.resize(32*16 + 1);
            initialFrameBuffer.setRange(sim::DefaultConfig::DEFINED, 0, 32*16);
            initialFrameBuffer.clearRange(sim::DefaultConfig::VALUE, 0, 32*16);
            
            initialFrameBuffer.set(sim::DefaultConfig::VALUE, 1);
            initialFrameBuffer.set(sim::DefaultConfig::VALUE, 3);
            initialFrameBuffer.set(sim::DefaultConfig::VALUE, 5);
            
            for (auto y : utils::Range(6, 9))
                initialFrameBuffer.setRange(sim::DefaultConfig::VALUE, y*32+10, 15);
            
            unsigned romReadDelay = 1;
            
            frontend::UnsignedInteger startAddress = 0b0000000000_uvec;
            MHDL_NAMED(startAddress);
            
            frontend::UnsignedInteger readAddress(10);
            MHDL_NAMED(readAddress);
            driveWith(readAddress, registerFactory(readAddress + 1_uvec, !initialized, startAddress));
            
            frontend::UnsignedInteger writeAddress = delay(registerFactory, readAddress, 1_bit, startAddress, romReadDelay);
            MHDL_NAMED(writeAddress);



            frontend::Bit romReadEnable = readAddress < 512_uvec;
            MHDL_NAMED(romReadEnable);
            frontend::Bit writeFromRom = delay(registerFactory, romReadEnable, 1_bit, 0_bit, romReadDelay);
            MHDL_NAMED(writeFromRom);

            frontend::BitVector romData(1);
            buildRom(clk, initialFrameBuffer, 
                    romReadEnable, readAddress, romData,
                        0b0_vec);
            MHDL_NAMED(romData);

            
            driveWith(initialized, !romReadEnable && !writeFromRom);
            
            buildDualPortRam(clk, clk, 32*16+1, 
                    mux(initialized, writeFromRom, ramWriteEnable),
                    mux(initialized, writeAddress, ramWriteAddress), 
                    mux(initialized, romData, ramWriteData), 
                    initialized & ramReadEnable, ramReadAddress, ramReadData,
                    0b0_vec);
            
        }
        MHDL_NAMED(ramWriteEnable);
        MHDL_NAMED(ramWriteAddress);
        MHDL_NAMED(ramWriteData);
        MHDL_NAMED(ramReadEnable);
        MHDL_NAMED(ramReadAddress);
        MHDL_NAMED(ramReadData);

        
        const unsigned totalDelay = 1 + 32 + 1;

        frontend::UnsignedInteger nextReadAddress = ramReadAddress + 1_uvec;
        nextReadAddress = mux(nextReadAddress < 512_uvec, 0b0000000000_uvec, nextReadAddress);
        
        driveWith(ramReadAddress, registerFactory(nextReadAddress, initialized, 0b0000000000_uvec));
        driveWith(ramWriteAddress, delay(registerFactory, ramReadAddress, 1_bit, 0b0000000000_uvec, totalDelay));
        driveWith(ramReadEnable, initialized);
        driveWith(ramWriteEnable, delay(registerFactory, 1_bit, initialized, 0_bit, totalDelay));
        
        
        GameOfLife game(32);

        BitStream in;
        MHDL_NAMED(in.data);
        MHDL_NAMED(in.valid);
        
        in.data = ramReadData[0];
        in.valid = delay(registerFactory, ramReadEnable, initialized, 0_bit, 1);

        BitStream out = game(registerFactory, in);
        
        frontend::BitVector castData(1);
        castData.setBit(0, out.data);
        driveWith(ramWriteData, castData);
    }
#endif

    design.getCircuit().cullUnnamedSignalNodes();
    design.getCircuit().cullOrphanedSignalNodes();

    VHDLExport vhdl("VHDL_out/");
    
    auto codeFormatting = (DefaultCodeFormatting*) vhdl.getFormatting();
    codeFormatting->addExternalNodeHandler([codeFormatting](std::ostream &file, const hlim::Node_External *node, unsigned indent, 
                                                            const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames, const std::vector<std::string> &clockNames)->bool{
        
        const SimpleDualPortRam *ram = dynamic_cast<const SimpleDualPortRam *>(node);
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

            if (!clockNames[SimpleDualPortRam::READ_CLK].empty())
                portmapList.push_back(std::string("RDCLK => ") + clockNames[SimpleDualPortRam::READ_CLK]);
            if (!clockNames[SimpleDualPortRam::WRITE_CLK].empty())
                portmapList.push_back(std::string("WRCLK => ") + clockNames[SimpleDualPortRam::WRITE_CLK]);
            portmapList.push_back("RST => reset");
            
            if (!inputSignalNames[SimpleDualPortRam::READ_ENABLE].empty())
                portmapList.push_back(std::string("RDEN => ") + inputSignalNames[SimpleDualPortRam::READ_ENABLE]);
            if (!inputSignalNames[SimpleDualPortRam::WRITE_ENABLE].empty())
                portmapList.push_back(std::string("WREN => ") + inputSignalNames[SimpleDualPortRam::WRITE_ENABLE]);
            if (!inputSignalNames[SimpleDualPortRam::WRITE_DATA].empty())
                portmapList.push_back(std::string("DI => ") + inputSignalNames[SimpleDualPortRam::WRITE_DATA]);
            if (!inputSignalNames[SimpleDualPortRam::READ_ADDR].empty())
                portmapList.push_back(std::string("RDADDR => ") + inputSignalNames[SimpleDualPortRam::READ_ADDR]);
            if (!inputSignalNames[SimpleDualPortRam::WRITE_ADDR].empty())
                portmapList.push_back(std::string("WRADDR => ") + inputSignalNames[SimpleDualPortRam::WRITE_ADDR]);
                
            if (!inputSignalNames[SimpleDualPortRam::READ_DATA].empty())
                portmapList.push_back(std::string("DO => ") + outputSignalNames[SimpleDualPortRam::READ_DATA]);

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
    });
    
    
    vhdl(design.getCircuit());

}
