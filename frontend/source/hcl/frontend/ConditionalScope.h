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

        bool m_isElsePart;
        hlim::NodePort m_condition;
        static thread_local hlim::NodePort m_lastCondition;
        std::set<hlim::Node_Multiplexer*> m_lastConditionMultiplexers;
        static thread_local std::set<hlim::Node_Multiplexer*> m_handoverLastConditionMultiplexers;
        
        void buildConditionalMuxes();
};


#define IF(x) \
    if (hcl::core::frontend::ConditionalScope ___condScope(x); true) 


#define ELSE \
    else { HCL_ASSERT(false); } if (hcl::core::frontend::ConditionalScope ___condScope{}; true)


}
    
