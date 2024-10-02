#include "gatery/scl_pch.h"
#include "XilinxPciStructs.h"

namespace gtry::scl::pci::xilinx
{
    CQUser CQUser::create(BitWidth streamW)
    {
        if (streamW == 512_b) {
            return CQUser{ ConstBVec(183_b) };
        }
        else if (streamW == 256_b || streamW == 128_b || streamW == 64_b) {
            return CQUser{ ConstBVec(88_b) };
        }
        else {
            HCL_DESIGNCHECK_HINT(false, "the selected width is not an option");
            return {};
        }
    }

    CCUser gtry::scl::pci::xilinx::CCUser::create(BitWidth streamW)
    {
        if (streamW == 512_b) {
            return CCUser{ ConstBVec(81_b) };
        }
        else if (streamW == 256_b || streamW == 128_b || streamW == 64_b) {
            return CCUser{ ConstBVec(33_b) };
        }
        else {
            HCL_DESIGNCHECK_HINT(false, "the selected width is not an option");
            return {};
        }
    }

    RQUser gtry::scl::pci::xilinx::RQUser::create(BitWidth streamW)
    {
        if (streamW == 512_b) {
            return RQUser{ ConstBVec(137_b) };
        }
        else if (streamW == 256_b || streamW == 128_b || streamW == 64_b) {
            return RQUser{ ConstBVec(62_b) };
        }
        else {
            HCL_DESIGNCHECK_HINT(false, "the selected width is not an option");
            return {};
        }
    }

    RCUser gtry::scl::pci::xilinx::RCUser::create(BitWidth streamW)
    {
        if (streamW == 512_b) {
            return RCUser{ ConstBVec(161_b) };
        }
        else if (streamW == 256_b || streamW == 128_b || streamW == 64_b) {
            return RCUser{ ConstBVec(75_b) };
        }
        else {
            HCL_DESIGNCHECK_HINT(false, "the selected width is not an option");
            return {};
        }
    }
}
