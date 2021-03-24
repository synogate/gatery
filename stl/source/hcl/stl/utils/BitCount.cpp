#include "BitCount.h"

#include <hcl/utils/BitManipulation.h>

namespace hcl::stl {

    core::frontend::BVec bitcount(core::frontend::BVec vec)
    {
        using namespace hcl::core::frontend;
        using namespace hcl::core::hlim;
        
        HCL_NAMED(vec);
        
        GroupScope entity(GroupScope::GroupType::ENTITY);
        entity
            .setName("bitcount")
            .setComment("Counts the number of high bits");
        
#if 1
        BVec sumOfOnes = BitWidth(utils::Log2C(vec.getWidth()+1));
        sumOfOnes = 0;
        for (auto i : utils::Range(vec.size()))
            sumOfOnes += zext(vec[i]);
        return sumOfOnes;
#else
        std::vector<BVec> subSums;
        subSums.resize(vec.getWidth());
        for (auto i : utils::Range(vec.size()))
            subSums[i] = zext(vec[i], utils::Log2C(vec.size() + 1));

        for (uint64_t i = utils::nextPow2(vec.getWidth().value)/2; i > 0; i /= 2) {
            for (uint64_t j = 0; j < i; j++) {
                if (j*2+0 >= subSums.size()) {
                    subSums[j] = 0;
                    continue;
                }
                if (j*2+1 >= subSums.size()) {
                    subSums[j] = subSums[j*2+0];
                    continue;
                }
                subSums[j] = subSums[j * 2 + 0] + subSums[j * 2 + 1];
            }
        }
        return subSums[0];
#endif
    }

 
}
