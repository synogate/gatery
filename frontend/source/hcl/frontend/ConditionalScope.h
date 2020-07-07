#pragma once

#include "Scope.h"


#include <hcl/utils/Traits.h>
#include <hcl/hlim/coreNodes/Node_PriorityConditional.h>

#include <vector>
#include <map>
#include <set>

namespace hcl::core::frontend {
    
class Bit;    
class ElementarySignal;

class ConditionalScope : public BaseScope<ConditionalScope>
{
    public:
        static ConditionalScope *get() { return m_currentScope; }

        ConditionalScope(const Bit &condition);
        ConditionalScope();
        ~ConditionalScope();

        inline const hlim::NodePort &getCondition() { return m_condition; }
        
        void registerConditionalAssignment(ElementarySignal *signal, hlim::NodePort previousOutput);
        void unregisterSignal(ElementarySignal *signal);
        inline bool isOutputConditional(hlim::NodePort output) { return m_conditional2previousOutput.contains(output); }
    protected:
        std::map<hlim::NodePort, hlim::NodePort> m_conditional2previousOutput;
        std::set<ElementarySignal*> m_conditionalyAssignedSignals;

        bool m_negateCondition;
        hlim::NodePort m_condition;
        static thread_local hlim::NodePort m_lastCondition;
        
        void buildConditionalMuxes();
};


#define IF(x) \
    if (hcl::core::frontend::ConditionalScope ___condScope(x); true) 


#define ELSE \
    else { HCL_ASSERT(false); } if (hcl::core::frontend::ConditionalScope ___condScope{}; true)


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
    
