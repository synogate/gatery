#include "Clock.h"

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

namespace mhdl::core::hlim {
    
ClockRational SignalDrivenClock::getAbsoluteFrequency() 
{
    MHDL_DESIGNCHECK_HINT(false, "Can not compute the absolute frequency of a signal driven clock!");
}

ClockRational SignalDrivenClock::getAbsolutePhaseShift() 
{
    MHDL_DESIGNCHECK_HINT(false, "Can not compute the absolute phase shift of a signal driven clock!");
}

ClockRational SignalDrivenClock::getFrequencyRelativeTo(BaseClock &other)
{
    
}

ClockRational SignalDrivenClock::getPhaseShiftRelativeTo(BaseClock &other)
{
    
}

    

RootClock::RootClock(std::string name, ClockRational frequency) : m_frequency(frequency)
{
    m_name = std::move(name);
}
    

ClockRational RootClock::getFrequencyRelativeTo(BaseClock &other)
{
    
}

ClockRational RootClock::getPhaseShiftRelativeTo(BaseClock &other)
{
    
}
    
    
    
    
ClockRational Clock::getAbsoluteFrequency()
{
    return m_parentClock->getAbsoluteFrequency() * m_parentRelativeMultiplicator;
}

ClockRational Clock::getAbsolutePhaseShift()
{
    return m_parentClock->getAbsolutePhaseShift() + m_parentRelativePhaseShift * m_parentClock->getAbsoluteFrequency();
}


ClockRational Clock::getFrequencyRelativeTo(BaseClock &other)
{
    
}

ClockRational Clock::getPhaseShiftRelativeTo(BaseClock &other)
{
    
}

        
}
