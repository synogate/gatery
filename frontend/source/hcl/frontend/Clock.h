#pragma once

#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"
#include "Reg.h"

#include <hcl/hlim/Clock.h>

#include <boost/optional.hpp>

#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>

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

            //Bit driveSignal();

            BVec operator() (const BVec& signal) const;
            BVec operator() (const BVec& signal, const BVec& reset) const;
            Bit operator() (const Bit& signal) const;
            Bit operator() (const Bit& signal, const Bit& reset) const;

            hlim::Clock *getClk() const { return m_clock; }
            hlim::ClockRational getAbsoluteFrequency() const { return m_clock->getAbsoluteFrequency(); }

            void setName(std::string name) { m_clock->setName(std::move(name)); }
        protected:
            hlim::Clock *m_clock;
            Clock(hlim::Clock *clock, const ClockConfig &config);

            void applyConfig(const ClockConfig &config);
    };


    class ClockScope : public BaseScope<ClockScope>
    {
        public:
            ClockScope(Clock &clock) : m_clock(clock) { }
            static Clock &getClk() {
                HCL_DESIGNCHECK_HINT(m_currentScope != nullptr, "No clock scope active!");
                return m_currentScope->m_clock;
            }
        protected:
            Clock &m_clock;
    };

    template<>
    struct Reg<BVec>
    {
        BVec operator () (const BVec& signal) { return ClockScope::getClk()(signal); }
        BVec operator () (const BVec& signal, const BVec& reset) { return ClockScope::getClk()(signal, reset); }
    };

    template<>
    struct Reg<Bit>
    {
        Bit operator () (const Bit& signal) { return ClockScope::getClk()(signal); }
        Bit operator () (const Bit& signal, const Bit& reset) { return ClockScope::getClk()(signal, reset); }
    };
}
