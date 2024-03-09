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

#include <queue>
#include <set>

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
		using promiseType = PromiseType;

		SmartCoroutineHandle() = default;

		~SmartCoroutineHandle() {
			reset();
		}

		void reset() {
			if (m_handle) {
				m_handle.promise().deregisterHandle();
				if (numReferences() == 0) // we are the last reference, destroy promise
					m_handle.destroy();
				m_handle = {};
			}
		}

		SmartCoroutineHandle(std::coroutine_handle<PromiseType> handle) : m_handle(handle) {
			if (m_handle)
				m_handle.promise().registerHandle();
		}

		SmartCoroutineHandle(const SmartCoroutineHandle<PromiseType> &other) : m_handle(other.m_handle) {
			if (m_handle)
				m_handle.promise().registerHandle();
		}

		SmartCoroutineHandle<PromiseType> &operator=(std::coroutine_handle<PromiseType> handle) {
			if (m_handle == handle) return *this;

			reset();
			m_handle = handle;
			if (m_handle)
				m_handle.promise().registerHandle();

			return *this;
		}

		SmartCoroutineHandle<PromiseType> &operator=(const SmartCoroutineHandle<PromiseType> &other) {
			if (&other == this) return *this;

			reset();
			m_handle = other.m_handle;
			if (m_handle)
				m_handle.promise().registerHandle();

			return *this;
		}

		operator bool() const { 
			return (bool) m_handle;
		}

		auto &promise() { return m_handle.promise(); }

		void resume() const {
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
		std::coroutine_handle<PromiseType> m_handle = {};
};


/**
 * @brief Type-erased version of SmartCoroutineHandle.
 */
template<>
class SmartCoroutineHandle<void>
{
	public:
		using returnType = void;

		SmartCoroutineHandle() = default;

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
			if (handle) {
				auto &promise = handle.promise();

				m_registerCallback = [&promise]() { promise.registerHandle(); };
				m_deregisterCallback = [&promise]() { promise.deregisterHandle(); };
				m_numReferencesCallback = [&promise]() { return promise.numReferences(); };

				m_registerCallback();
			}
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
			if (&other == this) return *this;

			reset();

			m_handle = other.m_handle;
			if (m_handle) {
				m_registerCallback = other.m_registerCallback;
				m_deregisterCallback = other.m_deregisterCallback;
				m_numReferencesCallback = other.m_numReferencesCallback;
				if (m_registerCallback)
					m_registerCallback();
			}

			return *this;
		}

		operator bool() const { 
			return (bool) m_handle;
		}

		void resume() const {
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


		bool operator<(const SmartCoroutineHandle<void> &rhs) const { 
			if (!m_handle) return (bool) rhs.m_handle;
			if (!rhs.m_handle) return true;
			return m_handle.address() < rhs.m_handle.address();
		}
	protected:
		std::function<void()> m_registerCallback;
		std::function<void()> m_deregisterCallback;
		std::function<size_t()> m_numReferencesCallback;
		std::coroutine_handle<> m_handle = {};
};

class SmartPromiseType
{
	public:
		~SmartPromiseType() {
			//HCL_ASSERT(m_numHandles == 0);
		}
		void registerHandle() { m_numHandles++; }
		void deregisterHandle() { HCL_ASSERT(m_numHandles > 0); m_numHandles--; }
		bool referenced() const { return m_numHandles != 0; }
		size_t numReferences() const { return m_numHandles; }
	protected:
		size_t m_numHandles = 0;
};

}


template<typename ReturnValue = void>
struct base_promise_type : public internal::SmartPromiseType {
	ReturnValue returnValue;
	template<std::convertible_to<ReturnValue> From>
	void return_value(From &&from) { returnValue = std::forward<From>(from); }
};

template<>
struct base_promise_type<void> : public internal::SmartPromiseType {
	void return_void() { }
};

template<typename ReturnValue, typename promise_type>
struct BaseCall {
	ReturnValue await_resume() noexcept { return calledSimulationCoroutine.promise().returnValue; }
	internal::SmartCoroutineHandle<promise_type> calledSimulationCoroutine;

	BaseCall(const internal::SmartCoroutineHandle<promise_type> &calledSimulationCoroutine) : calledSimulationCoroutine(calledSimulationCoroutine) { }
};

template<typename promise_type>
struct BaseCall<void, promise_type> {
	void await_resume() noexcept { }
	internal::SmartCoroutineHandle<promise_type> calledSimulationCoroutine;

	BaseCall(const internal::SmartCoroutineHandle<promise_type> &calledSimulationCoroutine) : calledSimulationCoroutine(calledSimulationCoroutine) { }
};


template<typename ReturnValue = void>
class SimulationFunction {
	public:
		struct promise_type : public base_promise_type<ReturnValue> {

			using returnType = ReturnValue;

			promise_type() = default;
			promise_type(const promise_type &) = delete;
			void operator=(const promise_type &) = delete;

			auto get_return_object() { return std::coroutine_handle<promise_type>::from_promise(*this); }
			auto initial_suspend() { return std::suspend_always(); }
			void unhandled_exception() { throw; }

			void* operator new(size_t size) { return std::malloc(size); }
			void operator delete(void *ptr) { std::free(ptr); }

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

			std::unique_ptr<std::function<SimulationFunction<ReturnValue>()>> functorInstance;
		};
		using Handle = internal::SmartCoroutineHandle<promise_type>;

		SimulationFunction() = default;
		SimulationFunction(const Handle &handle) : m_handle(handle) { }
		SimulationFunction(const std::coroutine_handle<promise_type> &handle) : m_handle(handle) { }

		inline const Handle &getHandle() const { return m_handle; }
		inline Handle &getHandle() { return m_handle; }

		/**
		 * @brief Awaiter for suspending a coroutine until another finishes.
		 * @details Unless the coroutine is to be joined has already finished, adds the calling coroutine to the list of coroutines awaiting final suspend of the one to be joined.
		 */
		struct Join : public BaseCall<ReturnValue, promise_type> {
			bool await_ready() noexcept { return BaseCall<ReturnValue, promise_type>::calledSimulationCoroutine.done(); }
			void await_suspend(std::coroutine_handle<> callingSimulationCoroutine) noexcept {
				BaseCall<ReturnValue, promise_type>::calledSimulationCoroutine.promise().awaitingFinalSuspend.push_back(callingSimulationCoroutine);
			}

			explicit Join(const Handle &handle) noexcept : BaseCall<ReturnValue, promise_type>(handle) { }
		};

  		/// Produces an awaiter if this SimulationFunction is co_awaited as a called sub-process of another SimulationFunction.
  		Join operator co_await() { // && noexcept { 
			m_handle.resume();
			return Join(m_handle);
		}
	protected:
		Handle m_handle;
};

class SimulationCoroutineHandler {
	public:
		static thread_local SimulationCoroutineHandler *activeHandler;

		~SimulationCoroutineHandler();

		template<typename ReturnValue>
		void start(const SimulationFunction<ReturnValue> &handle, bool runImmediate = false) {
			HCL_ASSERT(!handle.getHandle().done());
			m_simulationCoroutines.insert(handle.getHandle());
			if (runImmediate)
				handle.getHandle().resume();
			else
				readyToResume(handle.getHandle().rawHandle());		
		}
		void stopAll();

		void readyToResume(std::coroutine_handle<> handle) { m_coroutinesReadyToResume.push(handle); }
		void run();

		template<typename promise_type>
		void coroutineFinalSuspending(const std::coroutine_handle<promise_type> &handle) {
			auto key = internal::SmartCoroutineHandle<>(handle);
			m_simulationCoroutines.erase(key);
		}

	protected:
		std::set<internal::SmartCoroutineHandle<>> m_simulationCoroutines;
		std::queue<std::coroutine_handle<>> m_coroutinesReadyToResume;
};


template<typename ReturnValue>
void SimulationFunction<ReturnValue>::promise_type::FinalSuspendAwaiter::await_suspend(std::coroutine_handle<promise_type> handle) noexcept {
	auto *handler = SimulationCoroutineHandler::activeHandler;
	for (const auto &coro : handle.promise().awaitingFinalSuspend)
		handler->readyToResume(coro);
	handler->coroutineFinalSuspending(handle);
}


template<typename ReturnValue>
auto forkFunc(const SimulationFunction<ReturnValue> &simFunc)
{
	auto *handler = SimulationCoroutineHandler::activeHandler;
	handler->start(simFunc, true);
	return simFunc.getHandle();
}


template<typename ReturnValue>
auto forkFunc(std::function<SimulationFunction<ReturnValue>()> &&functor)
{
	// Create a "portable" copy of the functor before we invoke.
	auto functorInstance = std::make_unique<std::function<SimulationFunction<ReturnValue>()>>(std::move(functor));
	// Invoke the copy so that all internal references are wrt. to said copy.
	auto simFunc = (*functorInstance)();
	// Store the copy in the promise object to be kept alive as long as the promise/coroutine exists.
	simFunc.getHandle().promise().functorInstance = std::move(functorInstance);

	return forkFunc(simFunc);
}


extern template class SimulationFunction<void>;
extern template class SimulationFunction<int>;
extern template class SimulationFunction<size_t>;
extern template class SimulationFunction<bool>;

}
