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
#include "BlockRam.h"

#include "blockRam/XilinxSimpleDualPortBlockRam.h"

using namespace gtry::scl;
using namespace gtry;

/*
Stream<BVec> gtry::scl::simpleDualPortRam(Stream<WritePort>& write, Stream<BVec> readAddress, std::string_view name)
{
    HCL_ASSERT_HINT(write.address.size() == readAddress.size(), "not yet implemented");

    const size_t wordWidth = write.writeData.size();

    gtry::scl::blockram::XilinxSimpleDualPortBlockRam::DefaultBitVectorState initVec;
    initVec.resize(wordWidth * (1ull << write.address.size()));
    initVec.clearRange(gtry::sim::DefaultConfig::DEFINED, 0, initVec.size());

    using XilinxRam = gtry::scl::blockram::XilinxSimpleDualPortBlockRam;

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

    Stream<BVec> ret{ SignalReadPort(xram) };
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
