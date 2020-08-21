#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
    struct AvalonMM
    {
        AvalonMM() = default;
        AvalonMM(const AvalonMM&) = default;
        AvalonMM(size_t addressWidth, size_t dataWidth) :
            address(core::frontend::BitWidth{ addressWidth }),
            read('0'),
            write('0'),
            writeData(core::frontend::BitWidth{ dataWidth }),
            readData(core::frontend::BitWidth{ dataWidth }),
            readDataValid('0')
        {}

        core::frontend::BVec address;
        core::frontend::Bit read;
        core::frontend::Bit write;
        core::frontend::BVec writeData;
        core::frontend::BVec readData;
        core::frontend::Bit readDataValid;
    };
}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::AvalonMM, address, read, write, writeData, readData, readDataValid);
