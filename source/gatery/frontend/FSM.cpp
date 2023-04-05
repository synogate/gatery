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
#include "gatery/pch.h"
#include "FSM.h"

#include "Signal.h"
#include "Bit.h"
#include "BVec.h"
#include "Scope.h"
#include "ConditionalScope.h"
#include "Constant.h"
#include "SignalMiscOp.h"
#include "SignalCompareOp.h"
#include "Clock.h"

#include <gatery/utils/Exceptions.h>

namespace gtry::fsm {

thread_local FSM *FSM::m_fsmContext = nullptr;
	
	
FSM::FSM(const Clock &clock, const BaseState &startState) : 
	m_nextState(8_b)
{
	m_fsmContext = this;
	
	m_currentState = clock(m_nextState, "8b0");
	m_currentState.setName("fsm_state");
	
	m_nextState = m_currentState;

	m_state2encoding[&startState] = std::make_unique<UInt>(8_b);
	m_state2encoding[&startState]->setName(startState.m_name);
	m_state2id[&startState] = m_nextStateId++;
	m_unhandledStates.push_back(&startState);
	
	while (!m_unhandledStates.empty()) {
		m_currentlyConstructingState = m_unhandledStates.back();
		m_unhandledStates.pop_back();

		IF (m_currentState == *m_state2encoding[m_currentlyConstructingState]) {
			if (m_currentlyConstructingState->m_onActive)
				m_currentlyConstructingState->m_onActive();
		}
	}
	
	for (auto &pair : m_state2encoding.anyOrder())
		*pair.second = m_state2id[pair.first];
}


void FSM::delayedSwitch(const BaseState &nextState)
{
	if (!m_fsmContext->m_state2encoding.contains(&nextState)) {
		m_fsmContext->m_unhandledStates.push_back(&nextState);
		m_fsmContext->m_state2encoding[&nextState] = std::make_unique<UInt>(8_b);
		m_fsmContext->m_state2encoding[&nextState]->setName(nextState.m_name);
		m_fsmContext->m_state2id[&nextState] = m_fsmContext->m_nextStateId++;
	}	
	
	m_fsmContext->m_nextState = *m_fsmContext->m_state2encoding[&nextState];
	if (m_fsmContext->m_currentlyConstructingState->m_onExit)
		m_fsmContext->m_currentlyConstructingState->m_onExit();
}

void FSM::delayedSwitch(const DelayedState &nextState)
{
	delayedSwitch((const BaseState&) nextState);
	if (nextState.m_onEnter)
		nextState.m_onEnter();
}

void FSM::immediateSwitch(const ImmediateState &nextState)
{
	if (!m_fsmContext->m_state2encoding.contains(&nextState)) {
		m_fsmContext->m_unhandledStates.push_back(&nextState);
		m_fsmContext->m_state2encoding[&nextState] = std::make_unique<UInt>(8_b);
		m_fsmContext->m_state2encoding[&nextState]->setName(nextState.m_name);
		m_fsmContext->m_state2id[&nextState] = m_fsmContext->m_nextStateId++;
	}
	
	m_fsmContext->m_nextState = *m_fsmContext->m_state2encoding[&nextState];
	if (m_fsmContext->m_currentlyConstructingState->m_onExit)
		m_fsmContext->m_currentlyConstructingState->m_onExit();
	m_fsmContext->m_currentlyConstructingState = &nextState;
	if (nextState.m_onActive)
		nextState.m_onActive();
}

Bit FSM::isInState(const BaseState &state) 
{
	HCL_DESIGNCHECK_HINT(m_state2encoding.contains(&state), "State is unreachable in this FSM!");
	return m_currentState == *m_state2encoding[&state];
}

void delayedSwitch(const DelayedState &nextState)
{
	FSM::delayedSwitch(nextState);
}
void delayedSwitch(const ImmediateState &nextState)
{
	FSM::delayedSwitch(nextState);
}

void immediateSwitch(const ImmediateState &nextState)
{
	FSM::immediateSwitch(nextState);
}


}
