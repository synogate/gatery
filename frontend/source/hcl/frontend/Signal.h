/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/NodeIO.h>
#include <hcl/hlim/Node.h>

#include <hcl/utils/Enumerate.h>
#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>

#include <boost/format.hpp>

#include "BitWidth.h"

namespace hcl::core::frontend {

    class ConditionalScope;
    class SignalPort;

    enum class Expansion
    {
        none,
        zero,
        one,
        sign
    };

    struct SignalReadPort : hlim::NodePort
    {
        SignalReadPort() = default;
        SignalReadPort(const SignalReadPort&) = default;
        explicit SignalReadPort(hlim::BaseNode* node, Expansion policy = Expansion::none) : hlim::NodePort{.node = node, .port = 0}, expansionPolicy(policy) {}
        explicit SignalReadPort(hlim::NodePort np, Expansion policy = Expansion::none) : hlim::NodePort(np), expansionPolicy(policy) {}

        Expansion expansionPolicy = Expansion::none;

        SignalReadPort expand(size_t width, hlim::ConnectionType::Interpretation resultType) const;
    };

    inline const hlim::ConnectionType& connType(const SignalReadPort& port) { return port.node->getOutputConnectionType(port.port); }
    inline size_t width(const SignalReadPort& port) { return connType(port).width; }

    class ElementarySignal {
    public:
        using isSignal = void;
        using isElementarySignal = void;
        
        ElementarySignal();
        ElementarySignal(const ElementarySignal&) noexcept = delete;
        virtual ~ElementarySignal();

        virtual bool valid() const = 0;

        // these methods are undefined for invalid signals (uninitialized)
        virtual BitWidth getWidth() const = 0;
        virtual hlim::ConnectionType getConnType() const = 0;
        virtual SignalReadPort getReadPort() const = 0;
        virtual std::string_view getName() const = 0;
        virtual void setName(std::string name) = 0;

    protected:
        size_t m_initialScopeId = 0;

    };

}
