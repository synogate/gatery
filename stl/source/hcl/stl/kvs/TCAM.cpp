#include "TCAM.h"

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
