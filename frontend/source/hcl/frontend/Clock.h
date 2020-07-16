#pragma once

#include "Bit.h"
#include "Scope.h"

#include <hcl/hlim/Clock.h>

#include <boost/optional.hpp>

namespace hcl::core::frontend {
    
class Clock;
    
class ClockConfig
{
    public:
        using ClockRational = hlim::ClockRational;
        using TriggerEvent = hlim::Clock::TriggerEvent;
        using ResetType = hlim::Clock::ResetType;
        
    protected:
        boost::optional<ClockRational> m_absoluteFrequency;
        boost::optional<ClockRational> m_frequencyMultiplier;
        boost::optional<std::string> m_name;
        boost::optional<std::string> m_resetName;
        boost::optional<TriggerEvent> m_triggerEvent;
        boost::optional<ResetType> m_resetType;
        boost::optional<bool> m_initializeRegs;
        boost::optional<bool> m_resetHighActive;
        boost::optional<bool> m_phaseSynchronousWithParent;
    
        friend class Clock;
    public:
#define BUILD_SET(varname, settername) \
        inline ClockConfig &settername(decltype(varname)::value_type v) { varname = std::move(v); return *this; }        

        BUILD_SET(m_absoluteFrequency, setAbsoluteFrequency)
        BUILD_SET(m_frequencyMultiplier, setFrequencyMultiplier)
        BUILD_SET(m_name, setName)
        BUILD_SET(m_resetName, setResetName)
        BUILD_SET(m_triggerEvent, setTriggerEvent)
        BUILD_SET(m_initializeRegs, setInitializeRegs)
        BUILD_SET(m_resetType, setResetType)
        BUILD_SET(m_resetHighActive, setResetHighActive)
        BUILD_SET(m_phaseSynchronousWithParent, setPhaseSynchronousWithParent)        
#undef BUILD_SET
};
    
    
/**
 * @todo write docs
 */
class Clock
{
    public:
        using ClockRational = hlim::ClockRational;
        using TriggerEvent = hlim::Clock::TriggerEvent;
        using ResetType = hlim::Clock::ResetType;
        
        Clock(const ClockConfig &config);
        Clock deriveClock(const ClockConfig &config);
        
        Clock(const Clock &other);
        Clock &operator=(const Clock &other);
        
        Bit driveSignal();
        
        hlim::Clock *getClk() const { return m_clock; }
    protected:
        hlim::Clock *m_clock;
        Clock(hlim::Clock *clock, const ClockConfig &config);
        
        void applyConfig(const ClockConfig &config);
};


class ClockScope : public BaseScope<ClockScope>
{
    public:
        ClockScope(Clock &clock) : m_clock(clock) { }
        static Clock &getClk() { return m_currentScope->m_clock; }
    protected:
        Clock &m_clock;
};


/*
inline void setName(Clock &clock, std::string name) {
    clock.setName(std::move(name));
}
*/
}








