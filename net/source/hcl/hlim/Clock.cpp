#include "Clock.h"

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

#include "../hlim/Node.h"

namespace hcl::core::hlim {
    
Clock::Clock()
{
    m_name = "clk";
    m_resetName = "reset";
    m_triggerEvent = TriggerEvent::RISING;
    m_resetType = ResetType::SYNCHRONOUS;
    m_resetHighActive = true;
    m_phaseSynchronousWithParent = false;
    m_parentClock = nullptr;
}

    
Clock::~Clock()
{
    while (!m_clockedNodes.empty())
        m_clockedNodes.front().node->detachClock(m_clockedNodes.front().port);
}
/*
Clock &Clock::createClockDivider(ClockRational frequencyDivider, ClockRational phaseShift = 0)
{
    assert(false);
    return 
}
*/


RootClock::RootClock(std::string name, ClockRational frequency) : m_frequency(frequency)
{
    m_name = std::move(name);
}
    

ClockRational RootClock::getFrequencyRelativeTo(Clock &other)
{
    return {};
}

    
DerivedClock::DerivedClock(Clock *parentClock)
{
    m_parentClock = parentClock;
    m_parentRelativeMultiplicator = 1;
    
    m_name = m_parentClock->getName();
    
    m_resetName = m_parentClock->getResetName();
    m_triggerEvent = m_parentClock->getTriggerEvent();
    m_resetType = m_parentClock->getResetType();
    m_resetHighActive = m_parentClock->getResetHighActive();
    m_phaseSynchronousWithParent = m_parentClock->getPhaseSynchronousWithParent();
}

    
ClockRational DerivedClock::getAbsoluteFrequency()
{
    return m_parentClock->getAbsoluteFrequency() * m_parentRelativeMultiplicator;
}

ClockRational DerivedClock::getFrequencyRelativeTo(Clock &other)
{
    return {};
}
        
}
