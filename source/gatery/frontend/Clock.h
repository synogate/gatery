/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"
#include "Reg.h"
#include "../hlim/Attributes.h"

#include <gatery/hlim/Clock.h>

#include <boost/optional.hpp>

#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>

namespace gtry {

    class Clock;

    hlim::ClockRational clockFromString(std::string text);

    class ClockConfig
    {
        public:
            using ClockRational = hlim::ClockRational;
            using TriggerEvent = hlim::Clock::TriggerEvent;
            using ResetType = hlim::RegisterAttributes::ResetType;
            using UsageType = hlim::RegisterAttributes::UsageType;
            using ResetActive = hlim::RegisterAttributes::Active;

            void loadConfig(const utils::ConfigTree& config);
            void print(std::ostream& s) const;

            const auto& absoluteFrequency() const { return m_absoluteFrequency; }
            const auto& frequencyMultiplier() const { return m_frequencyMultiplier; }
            const auto& name() const { return m_name; }
            const auto& resetName() const { return m_resetName; }
            const auto& triggerEvent() const { return m_triggerEvent; }
            const auto& phaseSynchronousWithParent() const { return m_phaseSynchronousWithParent; }
            const auto& resetType() const { return m_resetType; }
            const auto& initializeRegs() const { return m_initializeRegs; }
            const auto& resetActive() const { return m_resetActive; }
            const auto& registerResetPinUsage() const { return m_registerResetPinUsage; }
            const auto& registerEnablePinUsage() const { return m_registerEnablePinUsage; }
            const auto& attributes() const { return m_attributes; }

        protected:
            boost::optional<ClockRational> m_absoluteFrequency;
            boost::optional<ClockRational> m_frequencyMultiplier;
            boost::optional<std::string> m_name;
            boost::optional<std::string> m_resetName;
            boost::optional<TriggerEvent> m_triggerEvent;
            boost::optional<bool> m_phaseSynchronousWithParent;

            boost::optional<ResetType> m_resetType;
            boost::optional<ResetType> m_memoryResetType;
            boost::optional<bool> m_initializeRegs;
<<<<<<< HEAD
            boost::optional<bool> m_resetHighActive;
=======
            boost::optional<bool> m_initializeMemory;
            boost::optional<ResetActive> m_resetActive;
>>>>>>> origin/master

        	boost::optional<UsageType> m_registerResetPinUsage;
	        boost::optional<UsageType> m_registerEnablePinUsage;

            hlim::Attributes m_attributes;

            friend class Clock;
        public:
            inline ClockConfig &addRegisterAttribute(const std::string &vendor, const std::string &attrib, const std::string &type, const std::string &value) { 
                m_attributes.userDefinedVendorAttributes[vendor][attrib] = {.type = type, .value = value};
                return *this; 
            }

    #define BUILD_SET(varname, settername) \
            inline ClockConfig &settername(decltype(varname)::value_type v) { varname = std::move(v); return *this; }

            BUILD_SET(m_absoluteFrequency, setAbsoluteFrequency)
            BUILD_SET(m_frequencyMultiplier, setFrequencyMultiplier)
            BUILD_SET(m_name, setName)
            BUILD_SET(m_resetName, setResetName)
            BUILD_SET(m_triggerEvent, setTriggerEvent)
            BUILD_SET(m_phaseSynchronousWithParent, setPhaseSynchronousWithParent)

            BUILD_SET(m_resetType, setResetType)
            BUILD_SET(m_memoryResetType, setMemoryResetType)
            BUILD_SET(m_initializeRegs, setInitializeRegs)
            BUILD_SET(m_initializeMemory, setInitializeMemory)
            BUILD_SET(m_resetActive, setResetActive)

            BUILD_SET(m_registerResetPinUsage, setRegisterResetPinUsage)
            BUILD_SET(m_registerEnablePinUsage, setRegisterEnablePinUsage)

    #undef BUILD_SET
    };

    std::ostream& operator << (std::ostream&, const ClockConfig&);

    /**
     * @todo write docs
     */
    class Clock
    {
        public:
            using ClockRational = hlim::ClockRational;
            using TriggerEvent = hlim::Clock::TriggerEvent;
            using ResetType = hlim::RegisterAttributes::ResetType;

            Clock(size_t freq);
            Clock(const ClockConfig &config);
            Clock deriveClock(const ClockConfig &config);

            Clock(const Clock &other);
            Clock &operator=(const Clock &other);

            Clock(hlim::Clock *clock) : m_clock(clock) { }

            //Bit driveSignal();

            BVec operator() (const BVec& signal, const RegisterSettings &settings = {}) const;
            BVec operator() (const BVec& signal, const BVec& reset, const RegisterSettings &settings = {}) const;
            Bit operator() (const Bit& signal, const RegisterSettings &settings = {}) const;
            Bit operator() (const Bit& signal, const Bit& reset, const RegisterSettings &settings = {}) const;

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
            static bool anyActive() { return m_currentScope != nullptr; }
        protected:
            Clock &m_clock;
    };

    template<>
    struct Reg<BVec>
    {
        BVec operator () (const BVec& signal, const RegisterSettings &settings = {}) { return ClockScope::getClk()(signal, settings); }
        BVec operator () (const BVec& signal, const BVec& reset, const RegisterSettings &settings = {}) { return ClockScope::getClk()(signal, reset, settings); }
    };

    template<>
    struct Reg<Bit>
    {
        Bit operator () (const Bit& signal, const RegisterSettings &settings = {}) { return ClockScope::getClk()(signal, settings); }
        Bit operator () (const Bit& signal, const Bit& reset, const RegisterSettings &settings = {}) { return ClockScope::getClk()(signal, reset, settings); }
    };
}
