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
        
#if 0
        BVec sumOfOnes = "b0000";
        for (auto i : utils::Range(data.getWidth()))
            sumOfOnes += data[i].zext(1);
        return sumOfOnes;
#else
        std::vector<BVec> subSums;
        subSums.resize(vec.getWidth());
        for (auto i : utils::Range(vec.getWidth()))
            subSums[i] = zext(vec[i], utils::Log2C(vec.size() + 1));

        for (unsigned i = utils::nextPow2(vec.getWidth())/2; i > 0; i /= 2) {
            for (unsigned j = 0; j < i; j++) {
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
