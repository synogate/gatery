#pragma once

#include "Scope.h"

#include <hcl/utils/Traits.h>
#include <hcl/hlim/coreNodes/Node_PriorityConditional.h>

#include <vector>
#include <map>
#include <set>
#include <optional>

namespace hcl::core::frontend {
    
    class Bit;

    class ConditionalScope : public BaseScope<ConditionalScope>
    {
        public:
            static ConditionalScope *get() { return m_currentScope; }

            ConditionalScope(const Bit &condition);
            ConditionalScope();
            ~ConditionalScope();

            static const Bit& getCurrentCondition();
            static hlim::NodePort getCurrentConditionPort() { return ConditionalScope::get()->m_fullCondition; }

        private:
            void setCondition(hlim::NodePort port);

            hlim::NodePort m_condition;
            hlim::NodePort m_fullCondition;

            thread_local static hlim::NodePort m_lastCondition;
    };


#define IF(x) \
    if (hcl::core::frontend::ConditionalScope ___condScope(x); true) 


#define ELSE \
    else { HCL_ASSERT(false); } if (hcl::core::frontend::ConditionalScope ___condScope{}; true)


}
    
