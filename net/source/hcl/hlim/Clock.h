#pragma once

#include "NodeIO.h"

#include <boost/rational.hpp>

#include <string>

namespace hcl::core::hlim {
    
using ClockRational = boost::rational<std::uint64_t>;    
    
class BaseClock
{
    public:
        virtual ~BaseClock();
        
        virtual ClockRational getAbsoluteFrequency() = 0;
        virtual ClockRational getAbsolutePhaseShift() = 0;

        virtual ClockRational getFrequencyRelativeTo(BaseClock &other) = 0;
        virtual ClockRational getPhaseShiftRelativeTo(BaseClock &other) = 0;
        
        inline const std::string &getName() const { return m_name; }
    protected:
        std::string m_name;
        
        std::vector<NodePort> m_clockedNodes;
        friend class BaseNode;        
};

class SignalDrivenClock : public BaseClock
{
    public:
        virtual ClockRational getAbsoluteFrequency() override;
        virtual ClockRational getAbsolutePhaseShift() override;

        virtual ClockRational getFrequencyRelativeTo(BaseClock &other) override;
        virtual ClockRational getPhaseShiftRelativeTo(BaseClock &other) override;
    protected:
        NodePort m_src;
};

class RootClock : public BaseClock
{
    public:
        RootClock(std::string name, ClockRational frequency);
        
        virtual ClockRational getAbsoluteFrequency() override { return m_frequency; }
        virtual ClockRational getAbsolutePhaseShift() override { return ClockRational(0); }

        virtual ClockRational getFrequencyRelativeTo(BaseClock &other) override;
        virtual ClockRational getPhaseShiftRelativeTo(BaseClock &other) override;
    protected:
        ClockRational m_frequency;
};

class Clock : public BaseClock
{
    public:
        virtual ClockRational getAbsoluteFrequency() override;
        virtual ClockRational getAbsolutePhaseShift() override;

        virtual ClockRational getFrequencyRelativeTo(BaseClock &other) override;
        virtual ClockRational getPhaseShiftRelativeTo(BaseClock &other) override;
    protected:
        BaseClock *m_parentClock;
        ClockRational m_parentRelativeMultiplicator;
        ClockRational m_parentRelativePhaseShift;
};


}
