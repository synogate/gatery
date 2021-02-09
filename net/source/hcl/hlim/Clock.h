#pragma once

#include "NodeIO.h"
#include "ClockRational.h"


#include <string>

namespace hcl::core::hlim {

class Clock
{
    public:
        enum class TriggerEvent {
            RISING,
            FALLING,
            RISING_AND_FALLING
        };
        
        enum class ResetType {
            SYNCHRONOUS,
            ASYNCHRONOUS,
            NONE
        };
        
        Clock();
        virtual ~Clock();
        
        virtual ClockRational getAbsoluteFrequency() = 0;
        virtual ClockRational getFrequencyRelativeTo(Clock &other) = 0;
        
        inline Clock *getParentClock() const { return m_parentClock; }
        
//        Clock &createClockDivider(ClockRational frequencyDivider, ClockRational phaseShift = 0);
        
        inline const std::string &getName() const { return m_name; }        
        inline const std::string &getResetName() const { return m_resetName; }
        inline const TriggerEvent &getTriggerEvent() const { return m_triggerEvent; }
        inline const ResetType &getResetType() const { return m_resetType; }
        inline const bool &getInitializeRegs() const { return m_initializeRegs; }
        inline const bool &getResetHighActive() const { return m_resetHighActive; }
        inline const bool &getPhaseSynchronousWithParent() const { return m_phaseSynchronousWithParent; }
        
        inline void setName(std::string name) { m_name = std::move(name); }
        inline void setResetName(std::string name) { m_resetName = std::move(name); }
        inline void setTriggerEvent(TriggerEvent trigEvt) { m_triggerEvent = trigEvt; }
        inline void setResetType(ResetType rstType) { m_resetType = rstType; }
        inline void setInitializeRegs(bool initializeRegs) { m_initializeRegs = initializeRegs; }
        inline void setResetHighActive(bool rstHigh) { m_resetHighActive = rstHigh; }
        inline void setPhaseSynchronousWithParent(bool phaseSync) { m_phaseSynchronousWithParent = phaseSync; }
        
    protected:
        Clock *m_parentClock = nullptr;
        
        std::string m_name;
        
        std::string m_resetName;
        TriggerEvent m_triggerEvent;
        ResetType m_resetType;
        bool m_initializeRegs;
        bool m_resetHighActive;
        bool m_phaseSynchronousWithParent;
        // todo:
        /*
            * clock enable
            * clock disable high/low
            * clock2signal
            */
        
        std::vector<NodePort> m_clockedNodes;
        friend class BaseNode;        
};

class RootClock : public Clock
{
    public:
        RootClock(std::string name, ClockRational frequency);
        
        virtual ClockRational getAbsoluteFrequency() override { return m_frequency; }
        virtual ClockRational getFrequencyRelativeTo(Clock &other) override;
        
        void setFrequency(ClockRational frequency) { m_frequency = m_frequency; }
    protected:
        ClockRational m_frequency;
};

class DerivedClock : public Clock
{
    public:
        DerivedClock(Clock *parentClock);
        
        virtual ClockRational getAbsoluteFrequency() override;
        virtual ClockRational getFrequencyRelativeTo(Clock &other) override;
        
        inline void setFrequencyMuliplier(ClockRational m) { m_parentRelativeMultiplicator = m; }
    protected:
        ClockRational m_parentRelativeMultiplicator;
};


}
