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

#include "NodeIO.h"
#include "ClockRational.h"
#include "Attributes.h"


#include <string>
#include <memory>

namespace gtry::hlim {

class DerivedClock;

class Clock
{
    public:
        enum class TriggerEvent {
            RISING,
            FALLING,
            RISING_AND_FALLING
        };


        Clock();
        virtual ~Clock();

        virtual ClockRational getAbsoluteFrequency() const = 0;
        virtual ClockRational getFrequencyRelativeTo(Clock &other) const = 0;

        inline Clock *getParentClock() const { return m_parentClock; }

//        Clock &createClockDivider(ClockRational frequencyDivider, ClockRational phaseShift = 0);

        inline const std::string &getName() const { return m_name; }        
        inline const std::string &getResetName() const { return m_resetName; }
        inline const TriggerEvent &getTriggerEvent() const { return m_triggerEvent; }
        inline const bool &getPhaseSynchronousWithParent() const { return m_phaseSynchronousWithParent; }

        inline void setName(std::string name) { m_name = std::move(name); }
        inline void setResetName(std::string name) { m_resetName = std::move(name); }
        inline void setTriggerEvent(TriggerEvent trigEvt) { m_triggerEvent = trigEvt; }
        inline void setPhaseSynchronousWithParent(bool phaseSync) { m_phaseSynchronousWithParent = phaseSync; }

        inline RegisterAttributes &getRegAttribs() { return m_registerAttributes; }
        inline const RegisterAttributes &getRegAttribs() const { return m_registerAttributes; }

        virtual std::unique_ptr<Clock> cloneUnconnected(Clock *newParent);

        inline void setMinResetTime(ClockRational time) { m_minResetTime = std::max(m_minResetTime, time); }
        inline void setMinResetCycles(size_t cycles) { m_minResetCycles = std::max(m_minResetCycles, cycles); }

        /// Returns the (reccursively determined) minimum time that this reset needs to be held.
        /// @details For clocks with asynchronous resets, this is at least one clock cycle. For synchronous clocks, this may be zero.
        ClockRational getMinResetTime() const;

        /// Returns the (reccursively determined) minimum number of cycles that this reset needs to be held.
        /// @details For clocks with synchronous resets, this is at least one.
        size_t getMinResetCycles() const;

        const std::vector<NodePort>& getClockedNodes() const;
        inline const std::vector<DerivedClock*> &getDerivedClocks() const { return m_derivedClocks; }
        inline void addDerivedClock(DerivedClock *clock) { m_derivedClocks.push_back(clock); }

        /// Which clock provides the actual clock signal for this clock
        /// @details For derived clocks, this may be the parent clock if the frequency, name, etc. is the same
        Clock *getClockPinSource();

        bool inheritsResetPinSource() const;

        /// Which clock provides the actual reset signal for this clock
        /// @details For derived clocks, this may be the parent clock if the name is the same
        Clock *getResetPinSource();
    protected:
        Clock *m_parentClock = nullptr;

        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) = 0;

        ClockRational m_minResetTime = 0;
        size_t m_minResetCycles = 0;

        std::string m_name;
        std::string m_resetName;
        TriggerEvent m_triggerEvent;
        bool m_phaseSynchronousWithParent;

        RegisterAttributes m_registerAttributes;

        // todo:
        /*
            * clock enable
            * clock disable high/low
            * clock2signal
            */
        
        std::set<NodePort> m_clockedNodes;
        mutable std::vector<NodePort> m_clockedNodesCache;
        std::vector<DerivedClock*> m_derivedClocks;
        friend class BaseNode;        
};

class RootClock : public Clock
{
    public:
        RootClock(std::string name, ClockRational frequency);

        virtual ClockRational getAbsoluteFrequency() const override { return m_frequency; }
        virtual ClockRational getFrequencyRelativeTo(Clock &other) const override;

        void setFrequency(ClockRational frequency) { m_frequency = frequency; }

        virtual std::unique_ptr<Clock> cloneUnconnected(Clock *newParent) override;
    protected:
        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) override;

        ClockRational m_frequency;
};

class DerivedClock : public Clock
{
    public:
        DerivedClock(Clock *parentClock);
        
        virtual ClockRational getAbsoluteFrequency() const override;
        virtual ClockRational getFrequencyRelativeTo(Clock &other) const override;
        
        inline void setFrequencyMuliplier(ClockRational m) { m_parentRelativeMultiplicator = m; }
        inline ClockRational getFrequencyMuliplier() const { return m_parentRelativeMultiplicator; }

        virtual std::unique_ptr<Clock> cloneUnconnected(Clock *newParent) override;
    protected:
        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) override;

        ClockRational m_parentRelativeMultiplicator;
};


}
