#include "FSM.h"

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"
#include "ConditionalScope.h"
#include "Registers.h"
#include "Constant.h"
#include "SignalMiscOp.h"
#include "SignalCompareOp.h"

#include <hcl/utils/Exceptions.h>

namespace hcl::core::frontend::fsm {

thread_local FSM *FSM::m_fsmContext = nullptr;
    
    
FSM::FSM(const Clock &clock, const BaseState &startState) : m_stateReg(0x00_bvec, clock)
{
    m_fsmContext = this;
    
    m_stateReg = m_stateReg.delay(1);
    m_stateReg.setName("fsm_state");

    m_state2encoding[&startState] = std::make_unique<BVec>(8);
    m_state2encoding[&startState]->setName(startState.m_name);
    m_state2id[&startState] = m_nextStateId++;
    m_unhandledStates.push_back(&startState);
    
    while (!m_unhandledStates.empty()) {
        m_currentState = m_unhandledStates.back();
        m_unhandledStates.pop_back();

        IF (m_stateReg.delay(1) == *m_state2encoding[m_currentState]) {
            if (m_currentState->m_onActive)
                m_currentState->m_onActive();
        }
    }
    
    for (auto &pair : m_state2encoding) {
        driveWith(*pair.second, ConstBVec(m_state2id[pair.first], 8));
    }
}


void FSM::delayedSwitch(const BaseState &nextState)
{
    if (!m_fsmContext->m_state2encoding.contains(&nextState)) {
        m_fsmContext->m_unhandledStates.push_back(&nextState);
        m_fsmContext->m_state2encoding[&nextState] = std::make_unique<BVec>(8);
        m_fsmContext->m_state2encoding[&nextState]->setName(nextState.m_name);
        m_fsmContext->m_state2id[&nextState] = m_fsmContext->m_nextStateId++;
    }    
    
    m_fsmContext->m_stateReg = *m_fsmContext->m_state2encoding[&nextState];
    if (m_fsmContext->m_currentState->m_onExit)
        m_fsmContext->m_currentState->m_onExit();
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
        m_fsmContext->m_state2encoding[&nextState] = std::make_unique<BVec>(8);
        m_fsmContext->m_state2encoding[&nextState]->setName(nextState.m_name);
        m_fsmContext->m_state2id[&nextState] = m_fsmContext->m_nextStateId++;
    }
    
    m_fsmContext->m_stateReg = *m_fsmContext->m_state2encoding[&nextState];
    if (m_fsmContext->m_currentState->m_onExit)
        m_fsmContext->m_currentState->m_onExit();
    m_fsmContext->m_currentState = &nextState;
    if (nextState.m_onActive)
        nextState.m_onActive();
}

Bit FSM::isInState(const BaseState &state) 
{
    HCL_DESIGNCHECK_HINT(m_state2encoding.contains(&state), "State is unreachable in this FSM!");
    return m_stateReg.delay(1) == *m_state2encoding[&state];
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


