#include "BlockRam.h"

#include "blockRam/XilinxSimpleDualPortBlockRam.h"

using namespace hcl::stl;
using namespace hcl::core::frontend;

/*
Stream<BVec> hcl::stl::simpleDualPortRam(Stream<WritePort>& write, Stream<BVec> readAddress, std::string_view name)
{
    HCL_ASSERT_HINT(write.address.size() == readAddress.size(), "not yet implemented");

    const size_t wordWidth = write.writeData.size();

    hcl::stl::blockram::XilinxSimpleDualPortBlockRam::DefaultBitVectorState initVec;
    initVec.resize(wordWidth * (1ull << write.address.size()));
    initVec.clearRange(hcl::core::sim::DefaultConfig::DEFINED, 0, initVec.size());

    using XilinxRam = hcl::stl::blockram::XilinxSimpleDualPortBlockRam;

    auto* clock = ClockScope::getClk().getClk();
    auto* xram = DesignScope::createNode<XilinxRam>(
        clock, clock, initVec, 
        wordWidth, wordWidth, false
    );


    write.ready = '1';
    readAddress.ready = '1';

    xram->connectInput(XilinxRam::WRITE_ADDR, write.address.getReadPort());
    xram->connectInput(XilinxRam::WRITE_DATA, write.writeData.getReadPort());
    xram->connectInput(XilinxRam::WRITE_ENABLE, write.valid.getReadPort());
    xram->connectInput(XilinxRam::READ_ADDR, readAddress.getReadPort());
    xram->connectInput(XilinxRam::READ_ENABLE, readAddress.valid.getReadPort());

    Stream<core::frontend::BVec> ret{ SignalReadPort(xram) };
    ret.valid = reg(readAddress.valid);

    auto nameSignal = [name](ElementarySignal& sig, std::string suffix) {
        if (sig.getName().empty())
            sig.setName(std::string(name) + '_' + suffix);
    };

    nameSignal(write.address, "wr_address");
    nameSignal(write.writeData, "wr_data");
    nameSignal(write.valid, "wr_en");
    nameSignal(write.ready, "wr_ready");
    nameSignal(readAddress, "rd_address");
    nameSignal(readAddress.valid, "rd_en");
    nameSignal(readAddress.ready, "rd_ready");
    nameSignal(ret, "rd_data");
    nameSignal(ret.valid, "rd_valid");
    nameSignal(ret.ready, "rd_ready");
    xram->setName(std::string(name));
    return ret;
}
*/