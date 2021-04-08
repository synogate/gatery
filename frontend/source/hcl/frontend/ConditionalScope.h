/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
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
    
