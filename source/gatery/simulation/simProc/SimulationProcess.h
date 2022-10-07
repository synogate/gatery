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
#include "../../utils/Exceptions.h"
#include "../../utils/Preprocessor.h"

namespace gtry::sim {

class Simulator;

namespace internal {

/**
 * @brief Coroutine handle with reference counting to automatically destroy the coroutine.
 */
template<typename PromiseType = void>
class SmartCoroutineHandle
{
	public:
		~SmartCoroutineHandle() {
			reset();
		}

		void reset() {
			m_handle.promise().deregisterHandle();
			if (numReferences() == 0) // we are the last reference, destroy promise
				m_handle.destroy();
			m_handle = {};
		}

		SmartCoroutineHandle(std::coroutine_handle<PromiseType> handle) : m_handle(handle) {
			m_handle.promise().registerHandle();
		}

		SmartCoroutineHandle(const SmartCoroutineHandle<PromiseType> &other) : m_handle(other.m_handle) {
			m_handle.promise().registerHandle();
		}

		SmartCoroutineHandle<PromiseType> &operator=(const SmartCoroutineHandle<PromiseType> &other) {
			reset();
			m_handle = other.m_handle;
			m_handle.promise().registerHandle();

			return *this;
		}

		operator bool() { 
			return (bool) m_handle;
		}

		auto promise() { return m_handle.promise(); }

		void resume() {
			if (m_handle)
				m_handle.resume();
		}

		bool done() const {
			if (!m_handle)
				return true;
			return m_handle.done();
		}

		size_t numReferences() const {
			if (!m_handle)
				return 0;
			return m_handle.promise().numReferences();
		}

		const std::coroutine_handle<PromiseType> &rawHandle() const { return m_handle; }
	protected:
		std::coroutine_handle<PromiseType> m_handle;
};


/**
 * @brief Type-erased version of SmartCoroutineHandle.
 */
template<>
class SmartCoroutineHandle<void>
{
	public:
		~SmartCoroutineHandle() {
			reset();
		}

		void reset() {
			if (m_handle) {
				m_deregisterCallback();
				if (m_numReferencesCallback() == 0) // we are the last reference, destroy promise
					m_handle.destroy();
				m_handle = {};

				m_registerCallback = {};
				m_deregisterCallback = {};
				m_numReferencesCallback = {};
			}
		}

		template<typename PromiseType> requires (!std::same_as<PromiseType, void>)
		SmartCoroutineHandle(std::coroutine_handle<PromiseType> handle) : m_handle(handle) {
			auto &promise = handle.promise();

			m_registerCallback = [&promise]() { promise.registerHandle(); };
			m_deregisterCallback = [&promise]() { promise.deregisterHandle(); };
			m_numReferencesCallback = [&promise]() { return promise.numReferences(); };

			m_registerCallback();
		}


		template<typename PromiseType> requires (!std::same_as<PromiseType, void>)
		SmartCoroutineHandle(const SmartCoroutineHandle<PromiseType> &other) : SmartCoroutineHandle(other.rawHandle()) { }

		SmartCoroutineHandle(const SmartCoroutineHandle<void> &other) : m_handle(other.rawHandle()) {
			m_registerCallback = other.m_registerCallback;
			m_deregisterCallback = other.m_deregisterCallback;
			m_numReferencesCallback = other.m_numReferencesCallback;
			if (m_registerCallback)
				m_registerCallback();
		}

		SmartCoroutineHandle<> &operator=(const SmartCoroutineHandle<> &other) {
			reset();

			m_registerCallback = other.m_registerCallback;
			m_deregisterCallback = other.m_deregisterCallback;
			m_numReferencesCallback = other.m_numReferencesCallback;
			if (m_registerCallback)
				m_registerCallback();

			return *this;
		}

		operator bool() { 
			return (bool) m_handle;
		}

		void resume() {
			if (m_handle)
				m_handle.resume();
		}

		bool done() const {
			if (!m_handle)
				return true;
			return m_handle.done();
		}

		size_t numReferences() const {
			if (!m_numReferencesCallback)
				return 0;
			return m_numReferencesCallback();
		}

		const std::coroutine_handle<> &rawHandle() const { return m_handle; }
	protected:
		std::function<void()> m_registerCallback;
		std::function<void()> m_deregisterCallback;
		std::function<size_t()> m_numReferencesCallback;
		std::coroutine_handle<> m_handle;
};

class SmartPromiseType
{
	public:
		~SmartPromiseType() {
			HCL_ASSERT(m_numHandles == 0);
		}
		void registerHandle() { m_numHandles++; }
		void deregisterHandle() { HCL_ASSERT(m_numHandles > 0); m_numHandles--; }
		bool referenced() const { return m_numHandles != 0; }
		size_t numReferences() const { return m_numHandles; }
	protected:
		size_t m_numHandles = 0;
};


/*
struct Join {
	bool await_ready() noexcept { return false; }
	auto await_suspend(std::coroutine_handle<> callingSimulationCoroutine) noexcept;
	void await_resume() noexcept { }

	Handle calledSimulationCoroutine;

	explicit Join(const SimulationCoroutine &simProc) noexcept;
};

struct Fork {
	bool await_ready() noexcept { return false; }
	auto await_suspend(std::coroutine_handle<> callingSimulationCoroutine) noexcept;
	void await_resume() noexcept { }

	Handle calledSimulationCoroutine;

	explicit Fork(const SimulationCoroutine &&simProc, SimulationCoroutineHandler *simProcHandler) noexcept;
};
*/

}


template<typename ReturnValue = void>
struct base_promise_type : public internal::SmartPromiseType {
	ReturnValue returnValue;
	template<std::convertible_to<ReturnValue> From>
	std::suspend_always return_value(From &&from) { returnValue = std::forward<From>(from); return {}; }
};

template<>
struct base_promise_type<void> : public internal::SmartPromiseType {
	void return_void() { }
};


template<typename ReturnValue = void>
class SimulationFunction {
	public:
		struct promise_type : public base_promise_type<ReturnValue> {
			auto get_return_object() { return std::coroutine_handle<promise_type>::from_promise(*this); }
			auto initial_suspend() { return std::suspend_always(); }
			void unhandled_exception() { throw; }

			/**
			 * @brief Special awaiter for the final suspend that potentially resumes the calling simulation processes.
			 * @details Resume doesn't happen directly, but by adding the awaiting handlers to the queue of ready coroutines of the SimulationCoroutineHandler.
			 */
			struct FinalSuspendAwaiter {
				bool await_ready() noexcept { return false; }
				/// Adds everything in promise_type::m_awaitingFinalSuspend to the ready queue of the SimulationCoroutineHandler.
				void await_suspend(std::coroutine_handle<promise_type> handle) noexcept;
				void await_resume() noexcept {}
			};
			auto final_suspend() noexcept { return FinalSuspendAwaiter{}; }

			std::vector<std::coroutine_handle<>> awaitingFinalSuspend;
		};
		using Handle = internal::SmartCoroutineHandle<promise_type>;

		SimulationFunction(const Handle &handle) : m_handle(handle) { }
		SimulationFunction(const std::coroutine_handle<promise_type> &handle) : m_handle(handle) { }

		inline const Handle &getHandle() const { return m_handle; }


		/**
		 * @brief Awaiter if this SimulationCoroutine is co_awaited as a sub-process of another SimulationCoroutine.
		 * @details Adds the calling coroutine to the list of coroutines awaiting final suspend of the called one.
		 */
		struct Call {
			bool await_ready() noexcept { return false; }
			void await_suspend(std::coroutine_handle<> callingSimulationCoroutine) noexcept { 
				calledSimulationCoroutine.promise().awaitingFinalSuspend.push_back(callingSimulationCoroutine); 
			}
			void await_resume() noexcept { }

			Handle calledSimulationCoroutine;

			explicit Call(const SimulationFunction<ReturnValue> &simProc) noexcept : calledSimulationCoroutine(simProc.getHandle()) { }
		};


  		/// Produces an awaiter if this SimulationCoroutine is co_awaited as a called sub-process of another SimulationCoroutine.
  		Call operator co_await() && noexcept { return {*this}; }

		//Fork fork() && noexcept;
	protected:
		Handle m_handle;
};



class SimulationCoroutineHandler {
	public:
		static thread_local SimulationCoroutineHandler *activeHandler;

		~SimulationCoroutineHandler();

		void start(const SimulationFunction<> &handle);
		void stopAll();

		void readyToResume(std::coroutine_handle<> handle) { m_coroutinesReadyToResume.push(handle); }
		void run();
	protected:
		void collectGarbage();
		std::vector<internal::SmartCoroutineHandle<>> m_simulationCoroutines;
		std::queue<std::coroutine_handle<>> m_coroutinesReadyToResume;
};


template<typename ReturnValue>
void SimulationFunction<ReturnValue>::promise_type::FinalSuspendAwaiter::await_suspend(std::coroutine_handle<promise_type> handle) noexcept {
	auto *handler = SimulationCoroutineHandler::activeHandler;
	for (const auto &coro : handle.promise().awaitingFinalSuspend)
		handler->readyToResume(coro);
}


}
