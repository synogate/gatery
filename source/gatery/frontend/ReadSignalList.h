/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include <gatery/simulation/simProc/SensitivityList.h>
#include <gatery/simulation/simProc/WaitChange.h>

namespace gtry {

/**
 * @addtogroup gtry_scopes
 * @addtogroup gtry_simProcs
 * @{
 */

/**
 * @brief A scope for simulation processes that collects all read signals and can subsequently be used to wait for any changes to the set.
 * @details This is used for building "combinatorical" behaviors in simulation processes. Such a simulation process can in in a while (true) loop
 * compute its outputs while an instance of ReadSignalList tracks all read inputs. Afterwards, the simulation process can suspend by co_awaiting 
 * ReadSignalList::anyInputChange and in this fashion reevaluate whenever any of the used inputs change.
 * @code
	{
		while (true) {
			ReadSignalList allInputs;
			simu(out) = simu(in1) + simu(in2);

			co_await allInputs.anyInputChange(); // Suspends execution until in1 or in2 change.
		}
	}
 * @endcode 
 */
class ReadSignalList : public BaseScope<ReadSignalList>
{
	public:
		sim::WaitChange anyInputChange();

		static void addToAllScopes(const hlim::NodePort &np);
		static ReadSignalList *get() { return m_currentScope; }
	protected:
		void addSignalRecursive(const hlim::NodePort &np);
		sim::SensitivityList m_list;
};

/**@}*/
/**@}*/

}
