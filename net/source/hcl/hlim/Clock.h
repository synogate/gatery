#pragma once

#include "NodeIO.h"

#include <boost/rational.hpp>

#include <string>

namespace hcl::core::hlim {
    
using ClockRational = boost::rational<std::uint64_t>;    
    
class Clock
{
    public:
        virtual ~Clock();
        
        virtual ClockRational getAbsoluteFrequency() = 0;
        virtual ClockRational getAbsolutePhaseShift() = 0;

        virtual ClockRational getFrequencyRelativeTo(Clock &other) = 0;
        virtual ClockRational getPhaseShiftRelativeTo(Clock &other) = 0;
        
        inline const std::string &getName() const { return m_name; }
        
//        Clock &createClockDivider(ClockRational frequencyDivider, ClockRational phaseShift = 0);
        
    protected:
        // reset config
        
        std::string m_name;
        
        std::vector<NodePort> m_clockedNodes;
        friend class BaseNode;        
};

class SignalDrivenClock : public Clock
{
    public:
        virtual ClockRational getAbsoluteFrequency() override;
        virtual ClockRational getAbsolutePhaseShift() override;

        virtual ClockRational getFrequencyRelativeTo(Clock &other) override;
        virtual ClockRational getPhaseShiftRelativeTo(Clock &other) override;
    protected:
        NodePort m_src;
};

class RootClock : public Clock
{
    public:
        RootClock(std::string name, ClockRational frequency);
        
        virtual ClockRational getAbsoluteFrequency() override { return m_frequency; }
        virtual ClockRational getAbsolutePhaseShift() override { return ClockRational(0); }

        virtual ClockRational getFrequencyRelativeTo(Clock &other) override;
        virtual ClockRational getPhaseShiftRelativeTo(Clock &other) override;
    protected:
        ClockRational m_frequency;
};

class ChildClock : public Clock
{
    public:
        virtual ClockRational getAbsoluteFrequency() override;
        virtual ClockRational getAbsolutePhaseShift() override;

        virtual ClockRational getFrequencyRelativeTo(Clock &other) override;
        virtual ClockRational getPhaseShiftRelativeTo(Clock &other) override;
    protected:
        Clock *m_parentClock;
        ClockRational m_parentRelativeMultiplicator;
        ClockRational m_parentRelativePhaseShift;
};


}
