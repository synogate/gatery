
#include "uart.h"

#include <metaHDL_core/frontend/Constant.h>
#include <metaHDL_core/frontend/Registers.h>
#include <metaHDL_core/frontend/BitVector.h>
#include <metaHDL_core/frontend/Integers.h>
#include <metaHDL_core/frontend/Scope.h>
#include <metaHDL_core/utils/Preprocessor.h>
#include <metaHDL_core/utils/StackTrace.h>
#include <metaHDL_core/utils/Exceptions.h>
#include <metaHDL_core/hlim/supportNodes/Node_SignalGenerator.h>
#include <metaHDL_core/simulation/BitVectorState.h>

#include <metaHDL_vis_Qt/MainWindowSimulate.h>
#include <QApplication>

#include <locale.h>


using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::core;
using namespace mhdl::utils;


class SignalGenerator : public Node_SignalGenerator
{
    public:
        SignalGenerator(BaseClock *clk) {
            m_clocks.resize(1);
            attachClock(clk, 0);

            resizeOutputs(2);
            setOutputConnectionType(0, {
                .interpretation = ConnectionType::BOOL,
                .width = 1,
            });
            setOutputConnectionType(1, {
                .interpretation = ConnectionType::RAW,
                .width = 8,
            });
            setOutputType(0, OUTPUT_LATCHED);
            setOutputType(1, OUTPUT_LATCHED);
        }
        
        void simulateReset(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
        {
            state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, 0);
            state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0], 1, ~0ull);    

            state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[1], 8, 0);
            state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[1], 8, ~0ull);    
            
            state.insertNonStraddling(sim::DefaultConfig::VALUE, internalOffsets[0], 64, 0);
            state.insertNonStraddling(sim::DefaultConfig::DEFINED, internalOffsets[0], 64, ~0ull);    
        }

        void simulateAdvance(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
        {
            std::uint64_t &tick = state.data(sim::DefaultConfig::VALUE)[internalOffsets[0]/64];
            switch (tick) {
                case 5:
                    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, 1);
                    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[1], 8, 0xCA);
                break;
                case 6:
                    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, 0);
                    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[1], 8, 0);
                break;
                case 10:
                    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, 1);
                    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[1], 8, 0xFE);
                break;
                case 20:
                    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, 0);
                    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[1], 8, 0);
                break;
            }
            tick++;
        }
        
        virtual std::string getTypeName() const override { return "SignalGenerator"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override {
            return "";
        }
        virtual std::string getOutputName(size_t idx) const override {
            switch (idx) {
                case 0: return "send";
                case 1: return "data";
            }
        }

        virtual std::vector<size_t> getInternalStateSizes() const override {
            return {64};
        }
};


int main(int argc, char *argv[]) { 
    setlocale(LC_ALL, "en_US.UTF-8");


    
    DesignScope design;

    {
        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all");
        
        auto clk = design.createClock<mhdl::core::hlim::RootClock>("clk", mhdl::core::hlim::ClockRational(10'000));
        RegisterConfig regConf = {.clk = clk, .resetName = "rst"};

        UartTransmitter uart(8, 1, 1000);
        
        /*
        BitVector data(8);
        
        Bit send, idle, outputLine;
        //data = 0b01010101_vec;
        //send = 1_bit;
        */
    
        auto sigGen = design.createNode<SignalGenerator>(clk);
        BitVector data({.node = sigGen, .port = 1ull});
        Bit send({.node = sigGen, .port = 0ull});
        
        Bit idle, outputLine;
        
        

        data.setName("data_of_uart0");
        send.setName("send_of_uart0");
        idle.setName("idle_of_uart0");
        outputLine.setName("outputLine_of_uart0");
        
        
        uart(data, send, outputLine, idle, regConf);
        
        Bit useIdle = idle;
        MHDL_NAMED(useIdle);
        Bit useOutputLine = outputLine;
        MHDL_NAMED(useOutputLine);
    }
    
    design.getCircuit().cullUnnamedSignalNodes();
    design.getCircuit().cullOrphanedSignalNodes();
            
    QApplication a(argc, argv);

    mhdl::vis::MainWindowSimulate w(nullptr, design.getCircuit());
    w.show();

    return a.exec();   
}
