/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
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
