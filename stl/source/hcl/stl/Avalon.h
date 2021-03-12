#pragma once
#include <hcl/frontend.h>
#include <map>
#include <list>
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

        void pinIn(std::string_view prefix);

        AvalonMM() = default;
        AvalonMM(AvalonMM&&) = default;
        AvalonMM(const AvalonMM&) = delete;
        void operator=(const AvalonMM&) = delete;
    };

    class AvalonNetworkSection
    {
    public:
        AvalonNetworkSection(std::string name = "");

        void add(std::string name, AvalonMM port);
        AvalonNetworkSection& addSection(std::string name);

        AvalonMM& find(std::string_view path);

        void assignPins();
    protected:
        std::string m_name;
        std::vector<std::pair<std::string, AvalonMM>> m_port;
        std::list<AvalonNetworkSection> m_subSections;
    };
}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::AvalonMM, address, ready, read, write, writeData, readData, readDataValid, readLatency, readyLatency, addressSel, dataSel);
