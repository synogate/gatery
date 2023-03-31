/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

#include "Bit.h"

#include <gatery/hlim/NodePort.h>
#include <gatery/utils/Traits.h>
#include <gatery/utils/CppTools.h>

namespace gtry {
/**
 * @addtogroup gtry_frontend
 * @{
 */

	class Bit;
	class ConditionalScope;

	class EnableScope : public BaseScope<EnableScope>
	{
		public:
			enum Always { always };
			static EnableScope *get() { return m_currentScope; }

			EnableScope(const Bit &enableCondition);
			//EnableScope(const Bit &enableCondition, const Bit &resetCondition);
			EnableScope(Always);
			~EnableScope();

			static Bit getFullEnable();
			//static Bit getFullReset();

			hlim::NodePort getFullEnableCondition() const { return m_fullEnableCondition; }
			//hlim::NodePort getFullResetCondition() const { return m_fullResetCondition; }

			explicit operator bool() const { return false; }

			/// Empty constructor for use in other scopes that require late initialization via EnableScope::setup
			EnableScope() { }
			/// Not to be used directly, setup allows late initalizaiton when the enable scope is used in other scopes.
			void setup(const hlim::NodePort &enableCondition)  { setEnable(enableCondition, true); }
		private:
			void setEnable(const hlim::NodePort &enableCondition, bool checkParent);

			hlim::NodePort m_enableCondition;
			hlim::NodePort m_fullEnableCondition;
			//hlim::NodePort m_resetCondition;
			//hlim::NodePort m_fullResetCondition;
	};


#define ENIF(...) \
	if (gtry::EnableScope ___enableScope{__VA_ARGS__}) {} else

#define ENALWAYS \
	if (gtry::EnableScope ___enableScope{EnableScope::always}) {} else

/// @}

}
