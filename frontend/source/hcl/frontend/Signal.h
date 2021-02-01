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
        virtual ~ElementarySignal() = default;

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
