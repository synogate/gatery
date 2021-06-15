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

        virtual ClockRational getAbsoluteFrequency() = 0;
        virtual ClockRational getFrequencyRelativeTo(Clock &other) = 0;

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
    protected:
        Clock *m_parentClock = nullptr;

        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) = 0;

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

        virtual std::unique_ptr<Clock> cloneUnconnected(Clock *newParent) override;
    protected:
        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) override;

        ClockRational m_frequency;
};

class DerivedClock : public Clock
{
    public:
        DerivedClock(Clock *parentClock);
        
        virtual ClockRational getAbsoluteFrequency() override;
        virtual ClockRational getFrequencyRelativeTo(Clock &other) override;
        
        inline void setFrequencyMuliplier(ClockRational m) { m_parentRelativeMultiplicator = m; }

        virtual std::unique_ptr<Clock> cloneUnconnected(Clock *newParent) override;
    protected:
        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) override;

        ClockRational m_parentRelativeMultiplicator;
};


}
