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

#include <vector>
#include <optional>

namespace gtry {
    
    class BVec;

    class Bit;


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
        Bit(Bit&& rhs);
        Bit(const BitDefault &defaultValue);
        ~Bit();

        Bit(const SignalReadPort& port);
        Bit(hlim::Node_Signal* node, size_t offset, size_t initialScopeId); // alias Bit

        template<typename T, typename = std::enable_if_t<std::is_same_v<T, char> || std::is_same_v<T, bool>>>
        Bit(T v) {
            createNode();
            assign(v);
        }
    
        Bit& operator=(const Bit& rhs) { m_resetValue = rhs.m_resetValue; assign(rhs.getReadPort());  return *this; }
        Bit& operator=(Bit&& rhs);
        Bit& operator=(const BitDefault &defaultValue);

        template<typename T, typename = std::enable_if_t<std::is_same_v<T, char> || std::is_same_v<T, bool>>>
        Bit& operator=(T rhs) { assign(rhs); return *this; }

        void setExportOverride(const Bit& exportOverride);

        void setAttrib(hlim::SignalAttributes attributes);

        BitWidth getWidth() const final;
        hlim::ConnectionType getConnType() const final;
        SignalReadPort getReadPort() const final;
        SignalReadPort getOutPort() const final;
        std::string_view getName() const final;
        void setName(std::string name) override;
        void addToSignalGroup(hlim::SignalGroup *signalGroup);

        void setResetValue(bool v);
        void setResetValue(char v);
        std::optional<bool> getResetValue() const { return m_resetValue; }

        hlim::Node_Signal* getNode() { return m_node; }

        Bit final() const;

    protected:
        SignalReadPort rewireAlias(SignalReadPort port) const;

        void createNode();
        void assign(bool);
        void assign(char);
        virtual void assign(SignalReadPort in, bool ignoreConditions = false);

        bool valid() const final; // hide method since Bit is always valid
        SignalReadPort getRawDriver() const;

    private:
        hlim::Node_Signal* m_node = nullptr;
        size_t m_offset = 0;
        std::optional<bool> m_resetValue;
    };

}
