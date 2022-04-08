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

#include "Signal.h"
#include "Scope.h"

#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/Attributes.h>
#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Traits.h>

#include <vector>
#include <optional>


namespace gtry {


/**
 * @addtogroup gtry_signals Signals
 * @ingroup gtry_frontend
 * @{
 */
 
    class Bit;
    struct construct_from_t;


    class BitDefault {
        public:
            BitDefault(const Bit& rhs);
            template<typename T, typename = std::enable_if_t<std::is_same_v<T, char> || std::is_same_v<T, bool>>>
            BitDefault(T v) { assign(v); }

            hlim::NodePort getNodePort() const { return m_nodePort; }
        protected:
            void assign(bool);
            void assign(char);

            hlim::RefCtdNodePort m_nodePort;
    };

    
    class Bit : public ElementarySignal
    {
    public:
        using isBitSignal = void;
        
        Bit();
        Bit(const Bit& rhs);
        Bit(const Bit& rhs, construct_from_t&&);
        Bit(Bit&& rhs);
        Bit(const BitDefault &defaultValue);
        ~Bit() noexcept;

        Bit(const SignalReadPort& port, std::optional<bool> resetValue = std::nullopt);
        Bit(hlim::Node_Signal* node, size_t offset, size_t initialScopeId); // alias Bit

        template<BitLiteral T>
        Bit(T v) {
            createNode();
            assign(v);
        }
    
        Bit& operator=(const Bit& rhs) { m_resetValue = rhs.m_resetValue; assign(rhs.readPort());  return *this; }
        Bit& operator=(Bit&& rhs);
        Bit& operator=(const BitDefault &defaultValue);

        template<BitLiteral T>
        Bit& operator=(T rhs) { assign(rhs); return *this; }

        void exportOverride(const Bit& exportOverride);

        BitWidth width() const final;
        hlim::ConnectionType connType() const final;
        SignalReadPort readPort() const final;
        SignalReadPort outPort() const final;
        std::string_view getName() const final;
        void setName(std::string name) override;
        void addToSignalGroup(hlim::SignalGroup *signalGroup);

        void resetValue(bool v);
        void resetValue(char v);
        std::optional<bool> resetValue() const { return m_resetValue; }

        hlim::Node_Signal* node() { return m_node; }

        Bit final() const;

    protected:
        SignalReadPort rewireAlias(SignalReadPort port) const;

        void createNode();
        void assign(bool);
        void assign(char);
        virtual void assign(SignalReadPort in, bool ignoreConditions = false);

        bool valid() const final; // hide method since Bit is always valid
        SignalReadPort rawDriver() const;

    private:
        hlim::Node_Signal* m_node = nullptr;
        size_t m_offset = 0;
        std::optional<bool> m_resetValue;
    };

    // ovreload to implement reset value override
    struct RegisterSettings;
    Bit reg(const Bit& val, const RegisterSettings& settings);
    Bit reg(const Bit& val);

    class PipeBalanceGroup;
    Bit pipestage(const Bit& signal);
    Bit pipeinput(const Bit& signal, PipeBalanceGroup& group);

/**@}*/
}
