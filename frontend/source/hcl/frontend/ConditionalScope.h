#pragma once

#include "Scope.h"

#include <hcl/utils/Traits.h>
#include <hcl/hlim/coreNodes/Node_PriorityConditional.h>


namespace hcl::core::frontend {
    
    class Bit;

    class ConditionalScope : public BaseScope<ConditionalScope>
    {
        public:
            static ConditionalScope *get() { return m_currentScope; }

            ConditionalScope(const Bit &condition);
            ConditionalScope();
            ~ConditionalScope();

            hlim::NodePort getFullCondition() const { return m_fullCondition; }
            size_t getId() const { return m_id; }

        private:
            void setCondition(hlim::NodePort port);

            const size_t m_id;
            hlim::NodePort m_condition;
            hlim::NodePort m_fullCondition;
            bool m_isElseScope;

            thread_local static hlim::NodePort m_lastCondition;
            thread_local static size_t s_nextId;
    };


#define IF(x) \
    if (hcl::core::frontend::ConditionalScope ___condScope(x); true) 


#define ELSE \
    else { HCL_ASSERT(false); } \
    if (hcl::core::frontend::ConditionalScope ___condScope{}; true)


}
    
