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

#include "../../compat/CoroutineWrapper.h"
#include "../../utils/Exceptions.h"
#include "../../utils/Preprocessor.h"

namespace gtry::sim {


/**
 * @brief Condition variable similar to std::condition_variable which allows simulation coroutines to synchronize each other.
 */
class Condition
{
	public:
		struct ConditionAwaitable {
			Condition &condition;

			ConditionAwaitable(Condition &condition) : condition(condition) { }

			bool await_ready() noexcept { return false; }
			void await_resume() noexcept {}
			void await_suspend(std::coroutine_handle<> callingSimulationCoroutine) noexcept;
		};

		/// Schedules one simulation coroutine that is waiting on this condition to resume (if any is waiting).
		inline void notify_one() { notify_oldest(); }

		/// Schedules the oldest simulation coroutine that is waiting on this condition (the one that has been waiting the longest) to resume (if any is waiting).
		void notify_oldest();
		/// Schedules all simulation coroutines that are waiting on this condition to resume.
		void notify_all();

		/// Suspends execution of the calling (co_awaiting) coroutine.
		ConditionAwaitable wait() { return {*this}; }
	protected:
		std::queue<std::coroutine_handle<>> m_awaitingCoroutines;


		friend struct ConditionAwaitable;
};

}
