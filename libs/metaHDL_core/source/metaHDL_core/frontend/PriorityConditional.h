#pragma once

#include "Scope.h"
#include "../utils/Traits.h"
#include "../frontend/Bit.h"
#include "../hlim/coreNodes/Node_PriorityConditional.h"

#include <vector>

namespace mhdl::core::frontend {
    
template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
class PriorityConditional
{
    public:
        PriorityConditional<DataSignal> &addCondition(const Bit &enableSignal, const DataSignal &value) {
            m_choices.push_back({enableSignal, value});
            return *this;
        }

        DataSignal operator()(const DataSignal &defaultCase) {
            hlim::Node_PriorityConditional *node = DesignScope::createNode<hlim::Node_PriorityConditional>();
            node->recordStackTrace();
            node->connectDefault({.node = defaultCase.getNode(), .port = 0ull});
            
            for (auto &c : m_choices)
                node->addInput({.node = c.first.getNode(), .port = 0ull},
                            {.node = c.second.getNode(), .port = 0ull});

            return DataSignal({.node = node, .port = 0ull});
        }
    protected:
        std::vector<std::pair<Bit, DataSignal>> m_choices;
};







class ConditionalScope : public BaseScope<ConditionalScope>
{
    public:
        static ConditionalScope *get() { return m_currentScope; }
        
        ConditionalScope(const Bit &condition);
        
        inline const Bit &getCondition() { return m_condition; }
    protected:
        Bit m_condition;
};


class ConditionalScopeHelper
{
    public:
        ConditionalScopeHelper(const Bit &condition);
        ~ConditionalScopeHelper();
    protected:

};


#define IF(x) \
    if (ConditionalScopeHelper ___condScope(x)) 


#define ELSE \
    else { MHDL_ASSERT(false); } if ()



/*

UnsignedIntegerSignal sum = 0;

WHEN(condition) {
    for (auto &entity : entities) {
        sum1 += vodoo(entity);
        sum2 += vodoo(entity);
    }
}

WHEN (condition) {
    UnsignedIntegerSignal sum2 = 0;

    WHEN (condition) {
        sum1 = sum1 + entity[0];
        sum2 = sum2 + entity[0];
        sum1 = sum1 + entity[1];
        sum2 = sum2 + entity[1];
        sum1 = sum1 + entity[2];
        sum2 = sum2 + entity[2];
    }
    sum1 += sum2;
} ELSE {
}

else { assert(true); } if (ElseHelper b) 

*/

}
