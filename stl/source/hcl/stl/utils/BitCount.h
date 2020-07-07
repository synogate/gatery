#pragma once

#include <hcl/frontend.h>
#include <hcl/utils/Traits.h>

#include <iostream>

namespace hcl::stl {
 
template<typename SignalType, typename = std::enable_if_t<hcl::utils::isBitVectorSignal<SignalType>::value>>
core::frontend::UnsignedInteger bitcount(SignalType vec)
{
    using namespace hcl::core::frontend;
    using namespace hcl::core::hlim;
    
    HCL_NAMED(vec);
    
    GroupScope entity(NodeGroup::GRP_ENTITY);
    entity
        .setName("bitcount")
        .setComment("Counts the number of high bits");
     
#if 0
    UnsignedInteger sumOfOnes = 0b0000_uvec;
    for (auto i : utils::Range(data.getWidth()))
        sumOfOnes += mux(data[i], 0_uvec, 1_uvec); /// @todo this must work better
    return sumOfOnes;
#else
    std::vector<UnsignedInteger> subSums;
    subSums.resize(vec.getWidth());
    for (auto i : utils::Range(vec.getWidth()))
        subSums[i] = mux(vec[i], 0_uvec, 1_uvec); /// @todo this must work better

    for (unsigned i = utils::nextPow2(vec.getWidth())/2; i > 0; i /= 2) {
        for (unsigned j = 0; j < i; j++) {
            if (j*2+0 >= subSums.size()) {
                subSums[j] = 0_uvec;
                continue;
            }
            if (j*2+1 >= subSums.size()) {
                subSums[j] = subSums[j*2+0];
                continue;
            }
            subSums[j] = cat(0_bit, subSums[j*2+0]) + cat(0_bit, subSums[j*2+1]);
        }
    }
    return subSums[0];
#endif
}

    
}
