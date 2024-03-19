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

#include "../../hlim/NodePort.h"

#include "../../compat/CoroutineWrapper.h"

#include "SensitivityList.h"

namespace gtry::sim {

/**
 * @brief Suspends a simulation process until any of the signals in the sensitivity list changes state.
 * 
 */
class WaitChange {
	public:
		WaitChange(SensitivityList sensitivityList);

		bool await_ready() noexcept { return false; } // always force reevaluation
		void await_suspend(std::coroutine_handle<> handle);
		void await_resume() noexcept { }

		inline const SensitivityList &getSensitivityList() const { return m_sensitivityList; }
	protected:
		SensitivityList m_sensitivityList;
};

}
