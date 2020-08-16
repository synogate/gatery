#pragma once

#include <hcl/frontend.h>

#include <hcl/utils/BitManipulation.h>
#include <hcl/utils/Range.h>

#include <vector>


namespace hcl::stl
{
    
    template<typename Signal>
    Signal delay(Signal signal, unsigned delay) {
        for (auto i : utils::Range(delay))
            signal = reg(signal);
        return signal;
    }
    

    template<typename Signal, class Functor>
    Signal treeReduceImpl(const std::vector<Signal> &input, size_t depth, size_t registersRemaining, size_t registerInterval, Functor functor) {
        using namespace core::frontend;
        
        if (input.size() == 1)
            return delay(input.front(), registersRemaining);
        
        bool insertReg = false;
        if (registerInterval > 0)
            insertReg = registersRemaining > 0 && (depth % registerInterval) == 0;

        if (insertReg)
            registersRemaining--;
        
        Signal left = treeReduceImpl(std::vector<Signal>(input.begin(), input.begin()+input.size()/2),
                                    depth+1,
                                    registersRemaining,
                                    registerInterval,
                                    functor);
                                    
        Signal right = treeReduceImpl(std::vector<Signal>(input.begin()+input.size()/2, input.end()),
                                    depth+1,
                                    registersRemaining,
                                    registerInterval,
                                    functor);

        Signal res = functor(left, right);
        return delay(res, insertReg?1:0);
    }
    
    template<typename ContainerType, class Functor>
    typename ContainerType::value_type treeReduce(const ContainerType &input, size_t numRegisterSteps, Functor functor) {
        std::vector<typename ContainerType::value_type> inputValues(input.begin(), input.end());
        
        size_t treeDepth = utils::Log2C(inputValues.size());
        
        return treeReduceImpl(inputValues, 0, numRegisterSteps, numRegisterSteps>0?(treeDepth+numRegisterSteps-1) / numRegisterSteps:0, functor);        
    }

}

