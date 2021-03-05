#pragma once
#include <hcl/frontend.h>
#include <map>
#include <string_view>

namespace hcl::stl
{
    struct AvalonMM
    {
        BVec address;
        std::optional<Bit> ready;
        std::optional<Bit> read;
        std::optional<Bit> write;
        std::optional<BVec> writeData;
        std::optional<BVec> readData;
        std::optional<Bit> readDataValid;

        size_t readLatency = 0;
        size_t readyLatency = 0;

        std::map<std::string_view, Selection> addressSel;
        std::map<std::string_view, Selection> dataSel;
    };
}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::AvalonMM, address, ready, read, write, writeData, readData, readDataValid, readLatency, readyLatency, addressSel, dataSel);
