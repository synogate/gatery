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

#include "../hlim/NodePort.h"
#include "BitVectorState.h"

#include "../compat/CoroutineWrapper.h"

#include <any>

namespace gtry::sim {

class Simulator;
class WaitFor;
class WaitUntil;
class WaitClock;
class WaitChange;
class WaitStable;
class SigHandle;

class SimulationContext {
	public:
		SimulationContext();
		virtual ~SimulationContext();

		virtual void overrideRegister(const SigHandle &handle, const DefaultBitVectorState &state) = 0;
		virtual void overrideSignal(const SigHandle &handle, const ExtendedBitVectorState &state) = 0;
		virtual void getSignal(const SigHandle &handle, DefaultBitVectorState &state) = 0;

		virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) = 0;
		virtual void onWarning(const hlim::BaseNode *src, std::string msg) = 0;
		virtual void onAssert(const hlim::BaseNode *src, std::string msg) = 0;

		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor) = 0;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil) = 0;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock) = 0;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitChange &waitChange) = 0;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitStable &waitChange) = 0;

		virtual bool hasAuxData(std::string_view key) const = 0;
		virtual std::any& registerAuxData(std::string_view key, std::any data) = 0;
		virtual std::any& getAuxData(std::string_view key) = 0;

		static SimulationContext *current() { return m_current; }
		//static double nowNs();

		virtual Simulator *getSimulator() = 0;
	protected:
		SimulationContext *m_overshadowed = nullptr;
		thread_local static SimulationContext *m_current;
};

}
