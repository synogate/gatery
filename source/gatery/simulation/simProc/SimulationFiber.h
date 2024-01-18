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

#include "../../utils/Exceptions.h"
#include "../../utils/Preprocessor.h"

#include "SimulationProcess.h"

#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>


namespace gtry::sim {

	class SimulationFiber {
		public:
			class SimulationTerminated : public std::exception { };

			SimulationFiber(SimulationCoroutineHandler &coroutineHandler, std::function<void()> body);
			~SimulationFiber();

			void start();
			void terminate();

			static SimulationFiber *thisFiber() { return m_thisFiber; }

			template<typename ReturnValue>
			static ReturnValue awaitCoroutine(SimulationFunction<ReturnValue> coroutine);

			template<typename ReturnValue>
			static ReturnValue awaitCoroutine(std::function<SimulationFunction<ReturnValue>()> coroutine) { return awaitCoroutine<ReturnValue>(coroutine()); }
		protected:
			SimulationCoroutineHandler &m_coroutineHandler;

			static thread_local SimulationFiber *m_thisFiber;

			std::function<void()> m_body;
			std::thread m_thread;
			bool m_terminate = false;
			bool m_threadRunning = true;
			std::mutex m_mutex;
			std::condition_variable m_wakeMain;
			std::condition_variable m_wakeFiber;

			void suspend();
			void resume();
	};

	template<typename ReturnValue>
	ReturnValue SimulationFiber::awaitCoroutine(SimulationFunction<ReturnValue> coroutine)
	{
		auto *fiberToResume = m_thisFiber;
		ReturnValue result;

		auto callbackWrapper = [fiberToResume, &result, &coroutine]()mutable->SimulationFunction<void>{
			result = co_await coroutine;
			fiberToResume->resume();
		};

		auto &handler = fiberToResume->m_coroutineHandler;
		handler.start(callbackWrapper(), false);
		fiberToResume->suspend();
		return result;
	}


}
