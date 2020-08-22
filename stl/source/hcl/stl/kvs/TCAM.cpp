#include "TCAM.h"

#include "../utils/OneHot.h"
#include "../hardCores/AsyncRam.h"

using namespace hcl::stl;
using namespace hcl::core::frontend;

BVec hcl::stl::constructTCAMCell(
    const core::frontend::BVec& searchKey, 
    const core::frontend::Bit& write, 
    const std::vector<core::frontend::BVec>& writeData)
{
    const size_t addrWidth = 5;
    const size_t keyWidth = searchKey.size();
    const size_t numElements = writeData.size();

    HCL_DESIGNCHECK_HINT(keyWidth % addrWidth == 0, "TCAM Cell optimized for MLABs supports key width in multiple of 5 bit only");
    HCL_DESIGNCHECK_HINT(numElements % 20 == 0, "TCAM Cell optimized for MLABs supports number of entries in multiple of 20 only");
    
    for (const BVec& wd : writeData)
        HCL_DESIGNCHECK(wd.size() == keyWidth / addrWidth);

    BVec match(BitWidth{ numElements });
    HCL_NAMED(match);
    match = -1;

    std::vector<AvalonMM> rams(keyWidth / addrWidth);
    for (size_t i = 0; i < rams.size(); ++i)
    {
        rams[i] = AvalonMM(addrWidth, numElements);
        rams[i].address = searchKey(i * addrWidth, addrWidth);
        rams[i].read = '1';
        rams[i].write = write;

        for(size_t j = 0; j < numElements; ++j)
            rams[i].writeData[j] = writeData[j].at(i);

        asyncRam(rams[i]);
        match &= rams[i].readData;
    }
    HCL_NAMED(rams);

    return match;
}

void hcl::stl::LutTCAM::setLutSize(size_t addrWidth, size_t dataWidth)
{
    m_lutAddrWidth = addrWidth;
    m_lutDataWidth = dataWidth;
}

void hcl::stl::LutTCAM::setSize(size_t numElements, size_t bitsPerElement)
{
    m_numElements = numElements;
    m_bitsPerElement = bitsPerElement;
}

void hcl::stl::LutTCAM::setInput(const Valid<LutCAMSimpleStreamRequest>& in)
{
    HCL_DESIGNCHECK(m_bitsPerElement % m_lutAddrWidth == 0);
    HCL_DESIGNCHECK(m_numElements % m_lutDataWidth == 0);
    HCL_DESIGNCHECK(in.updateLutData.size() == m_lutDataWidth);

    SymbolSelect keyWord{ m_lutAddrWidth };
    m_valid = in.valid & !in.update;

    m_luts.reserve(getNumLuts());
    for (size_t i = 0; i < getNumLuts(); ++i)
    {
        AvalonMM& lut = m_luts.emplace_back(m_lutAddrWidth, m_lutDataWidth);
        lut.address = in.searchKey(keyWord[i % getNumLutsPerElement()]);
        lut.read = '1';
        lut.write = in.update & in.updateLutAddr == i;
        lut.writeData = in.updateLutData;
    }
}

void hcl::stl::LutTCAM::setMemoryType(std::function<void(AvalonMM&)> ramFactory)
{
    for (AvalonMM& lut : m_luts)
        ramFactory(lut);
    setName(m_luts, "luts");
}

void hcl::stl::LutTCAM::setPerElementReduce()
{
    std::vector<BVec> groupMatch;

    for (auto it = m_luts.begin(); it != m_luts.end();)
    {
        auto lut_end = it + getNumLutsPerElement();

        BVec match = it->readData;
        for (auto it2 = it + 1; it2 != lut_end; ++it2)
            match &= it2->readData;
        groupMatch.push_back(match);
    }
    setName(groupMatch, "perGroupMatch");

    m_match = pack(groupMatch);
    setName(m_match, "match");
}

Valid<BVec> hcl::stl::LutTCAM::getResultIndex()
{
    return {
        m_valid,
        priorityEncoder(m_match).index
    };
}
