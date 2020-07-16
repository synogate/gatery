#pragma once

#include "Bit.h"
#include "Registers.h"


#include <string>
#include <map>
#include <memory>
#include <vector>
#include <functional>


namespace hcl::core::frontend::fsm {

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
        const BaseState* m_currentState;
        Register<BVec> m_stateReg;
        size_t m_nextStateId = 0;
        std::map<const BaseState*, std::unique_ptr<BVec>> m_state2encoding;
        std::map<const BaseState*, size_t> m_state2id;
        
        static thread_local FSM *m_fsmContext;
};

void delayedSwitch(const DelayedState &nextState);
void delayedSwitch(const ImmediateState &nextState);
void immediateSwitch(const ImmediateState &nextState);

}


namespace hcl::core::frontend {
inline void setName(fsm::BaseState &state, std::string name) { state.setName(std::move(name)); }
}
