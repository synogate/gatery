#include "AsyncRam.h"

using namespace hcl::stl;
using namespace hcl::core::frontend;

void hcl::stl::asyncRam(AvalonMM& avmm)
{
    HCL_DESIGNCHECK(avmm.readData.size() == avmm.writeData.size());

    std::vector<BVec> ram(1ull << avmm.address.size());
    for (size_t i = 0; i < ram.size(); ++i)
    {
        ram[i] = BitWidth{ avmm.readData.size() };
        ram[i] = reg(ram[i]);
        IF(avmm.write & avmm.address == i)
            ram[i] = avmm.writeData;
    }
    HCL_NAMED(ram);

    avmm.readData = ConstBVec(avmm.readData.size());
    IF(avmm.read & ~avmm.write)
        avmm.readData = mux(avmm.address, ram);
    avmm.readDataValid = avmm.read;
}
