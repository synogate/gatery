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

#include "../../utils/CoroutineWrapper.h"

namespace gtry::sim {

class SimulationProcess {
	public:
		struct promise_type {
			auto get_return_object() { return std::coroutine_handle<promise_type>::from_promise(*this); }
			auto initial_suspend() { return std::suspend_always(); }
			void return_void() { }
			void unhandled_exception() { throw; }

			struct FinalSuspendAwaiter {
				bool await_ready() noexcept { return false; }

				void await_suspend(std::coroutine_handle<promise_type> handle) noexcept { 
					if (handle.promise().readyToContinue)
						handle.promise().potentialContinuation.resume();
					else
						handle.promise().readyToContinue = true;
				}

				void await_resume() noexcept {}
			};
			auto final_suspend() noexcept { return FinalSuspendAwaiter{}; }

			std::coroutine_handle<> potentialContinuation;
			bool readyToContinue = false;
		};
		using Handle = std::coroutine_handle<promise_type>;

		SimulationProcess(Handle handle);
		~SimulationProcess();

		SimulationProcess(const SimulationProcess&) = delete;
		SimulationProcess(SimulationProcess &&rhs) { m_handle = rhs.m_handle; rhs.m_handle = {}; }
		void operator=(const SimulationProcess&) = delete;

		void resume();


		struct Awaiter {
			bool await_ready() noexcept { return false; }
			bool await_suspend(std::coroutine_handle<> newHandle) noexcept {
				handle.promise().potentialContinuation = newHandle;
				handle.resume();

				bool wasReadyToContinue = handle.promise().readyToContinue;
				handle.promise().readyToContinue = true;
				return !wasReadyToContinue;
			}
			void await_resume() noexcept { }

			Handle handle;

			private:
				friend SimulationProcess;
				explicit Awaiter(Handle handle) noexcept : handle(handle) { }			
		};

  		Awaiter operator co_await() && noexcept;
	protected:
		Handle m_handle;
};

}
