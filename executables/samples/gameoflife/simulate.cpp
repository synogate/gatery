
#include "gameoflife.h"
#include "SimpleDualPortRam.h"

#include <metaHDL_core/frontend/Constant.h>
#include <metaHDL_core/frontend/Registers.h>
#include <metaHDL_core/frontend/BitVector.h>
#include <metaHDL_core/frontend/Integers.h>
#include <metaHDL_core/frontend/Scope.h>
#include <metaHDL_core/utils/Preprocessor.h>
#include <metaHDL_core/utils/StackTrace.h>
#include <metaHDL_core/utils/Exceptions.h>
#include <metaHDL_core/utils/BitManipulation.h>
#include <metaHDL_core/hlim/supportNodes/Node_SignalGenerator.h>
#include <metaHDL_core/simulation/BitVectorState.h>

#include <metaHDL_vis_Qt/MainWindowSimulate.h>
#include <QApplication>

#include <locale.h>


using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::core;
using namespace mhdl::utils;



int main(int argc, char *argv[]) { 
    setlocale(LC_ALL, "en_US.UTF-8");

    DesignScope design;

    {
        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all");

        auto clk = design.createClock<mhdl::core::hlim::RootClock>("clk", mhdl::core::hlim::ClockRational(10'000));
        RegisterConfig regConf = {.clk = clk, .resetName = "rst"};
        RegisterFactory registerFactory(regConf);
        

        frontend::Bit ramWriteEnable = 0_bit;
        frontend::UnsignedInteger ramWriteAddress(10);
        frontend::BitVector ramWriteData(1);

        frontend::Bit ramReadEnable = 0_bit;
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
                initialFrameBuffer.setRange(sim::DefaultConfig::VALUE, y*32+10, 20);
            
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

        GameOfLife game(32);

        BitStream in;
        MHDL_NAMED(in.data);
        MHDL_NAMED(in.valid);
        
        in.data = 1_bit;
        in.valid = 1_bit;

        BitStream out = game(registerFactory, in);
    }

    design.getCircuit().cullUnnamedSignalNodes();
    design.getCircuit().cullOrphanedSignalNodes();

            
    QApplication a(argc, argv);

    mhdl::vis::MainWindowSimulate w(nullptr, design.getCircuit());
    w.show();

    return a.exec();   
}
