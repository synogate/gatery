/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

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
#pragma once

#include "Scope.h"
#include "EnableScope.h"

#include <gatery/hlim/NodePort.h>
#include <gatery/utils/Traits.h>

namespace gtry 
{
/**
 * @addtogroup gtry_frontend
 * @{
 */
	class Bit;

	class ConditionalScope : public BaseScope<ConditionalScope>
	{
	public:
		struct ElseCase {};

		static ConditionalScope *get() { return m_currentScope; }

		ConditionalScope(const Bit &condition, bool override = false);
		ConditionalScope(ElseCase);
		ConditionalScope(ElseCase, const Bit& condition);
		~ConditionalScope();

		static Bit globalEnable();

		hlim::NodePort getFullCondition() const { return m_fullCondition; }
		size_t getId() const { return m_id; }

		explicit operator bool() const { return true; }

	private:
		void setCondition(hlim::NodePort port, bool override = false);

		const size_t m_id;
		hlim::NodePort m_condition;
		hlim::NodePort m_fullCondition;

		/// @todo: Remove once we decouple enable scopes and condition scopes
		EnableScope m_enScope;
		std::optional<hlim::NodePort> m_lastConditionOnEntry;
		std::optional<hlim::NodePort> m_combinedelseChainConditon;

		thread_local static hlim::NodePort m_lastCondition;
		thread_local static size_t s_nextId;
	};


#define IF(x) \
	if (gtry::ConditionalScope ___condScope{x}) 

#if __clang__
#define ELSE \
	if (gtry::ConditionalScope ___condScope{ConditionalScope::ElseCase{}})
#else
#define ELSE \
	else { HCL_ASSERT(false); } \
	if (gtry::ConditionalScope ___condScope{ConditionalScope::ElseCase{}})
#endif

	/// @brief This is preferred over the use of ELSE IF (with space) to avoid dangling else warnings on clang.
#define ELSEIF(x) \
	else { HCL_ASSERT(false); } \
	if (gtry::ConditionalScope ___condScope{ConditionalScope::ElseCase{}, x})
/// @}

}
