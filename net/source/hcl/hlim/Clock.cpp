#include "Clock.h"

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

#include "../hlim/Node.h"

namespace hcl::core::hlim {
    
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

    
ClockRational SignalDrivenClock::getAbsoluteFrequency() 
{
    HCL_DESIGNCHECK_HINT(false, "Can not compute the absolute frequency of a signal driven clock!");
}

ClockRational SignalDrivenClock::getAbsolutePhaseShift() 
{
    HCL_DESIGNCHECK_HINT(false, "Can not compute the absolute phase shift of a signal driven clock!");
}

ClockRational SignalDrivenClock::getFrequencyRelativeTo(Clock &other)
{
    return {};
}

ClockRational SignalDrivenClock::getPhaseShiftRelativeTo(Clock &other)
{
    return {};
}

    

RootClock::RootClock(std::string name, ClockRational frequency) : m_frequency(frequency)
{
    m_name = std::move(name);
}
    

ClockRational RootClock::getFrequencyRelativeTo(Clock &other)
{
    return {};
}

ClockRational RootClock::getPhaseShiftRelativeTo(Clock &other)
{
    return {};
}
    
    
    
    
ClockRational ChildClock::getAbsoluteFrequency()
{
    return m_parentClock->getAbsoluteFrequency() * m_parentRelativeMultiplicator;
}

ClockRational ChildClock::getAbsolutePhaseShift()
{
    return m_parentClock->getAbsolutePhaseShift() + m_parentRelativePhaseShift * m_parentClock->getAbsoluteFrequency();
}


ClockRational ChildClock::getFrequencyRelativeTo(Clock &other)
{
    return {};
}

ClockRational ChildClock::getPhaseShiftRelativeTo(Clock &other)
{
    return {};
}

        
}
