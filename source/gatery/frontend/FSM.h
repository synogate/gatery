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

#include "Bit.h"
#include "UInt.h"


#include <string>
#include <map>
#include <memory>
#include <vector>
#include <functional>

namespace gtry {
	class Clock;
}

namespace gtry::fsm {

class FSM;	
	
class BaseState
{
	public:
		virtual ~BaseState() = default;
		inline void onActive(std::function<void(void)> code) { m_onActive = std::move(code); }
		inline void onExit(std::function<void(void)> code) { m_onExit = std::move(code); }
		
		void setName(std::string name) { m_name = std::move(name); }
	protected:
		friend class FSM;
		std::string m_name;
		std::function<void(void)> m_onActive;
		std::function<void(void)> m_onExit;
};

class DelayedState : public BaseState
{
	public:
		virtual ~DelayedState() = default;
		void onEnter(std::function<void(void)> code) { m_onEnter = std::move(code); }
	protected:
		friend class FSM;
		std::function<void(void)> m_onEnter;
};

class ImmediateState : public BaseState
{
	public:
		virtual ~ImmediateState() = default;
	protected:
		friend class FSM;
};


/**
 * @todo write docs
 */
class FSM
{
	public:
		FSM(const Clock &clock, const BaseState &startState);
		Bit isInState(const BaseState &state);
		
		static void delayedSwitch(const BaseState &nextState);
		static void delayedSwitch(const DelayedState &nextState);
		static void immediateSwitch(const ImmediateState &nextState);
	protected:
		std::vector<const BaseState*> m_unhandledStates;
		const BaseState* m_currentlyConstructingState;
		UInt m_currentState;
		UInt m_nextState;
		size_t m_nextStateId = 0;
		utils::UnstableMap<const BaseState*, std::unique_ptr<UInt>> m_state2encoding;
		utils::UnstableMap<const BaseState*, size_t> m_state2id;
		
		static thread_local FSM *m_fsmContext;
};

void delayedSwitch(const DelayedState &nextState);
void delayedSwitch(const ImmediateState &nextState);
void immediateSwitch(const ImmediateState &nextState);

}


namespace gtry {
inline void setName(fsm::DelayedState&state, std::string name) { state.setName(std::move(name)); }
inline void setName(fsm::ImmediateState& state, std::string name) { state.setName(std::move(name)); }
}
