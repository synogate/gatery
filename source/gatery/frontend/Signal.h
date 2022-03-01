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

#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/NodeIO.h>
#include <gatery/hlim/Node.h>

#include <gatery/utils/Enumerate.h>
#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>

#include <boost/format.hpp>

#include "BitWidth.h"

namespace gtry {

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
        virtual SignalReadPort getOutPort() const = 0;
        virtual std::string_view getName() const = 0;
        virtual void setName(std::string name) = 0;

    protected:
        size_t m_initialScopeId = 0;

    };

    struct NormalizedWidthOperands
    {
        template<typename SigA, typename SigB>
        NormalizedWidthOperands(const SigA&, const SigB&);

        SignalReadPort lhs, rhs;
    };

    template<typename T, typename = void>
    struct is_signal : std::false_type
    {};
    
    //// base signal like Bit, BVec, UInt, SInt

    template<typename T>
    concept ReadableBaseSignal = requires(T v)
    {
        { v.getReadPort() } -> std::same_as<SignalReadPort>;
        { v.getName() } -> std::convertible_to<std::string_view>;
    };

    template<typename T>
    concept BaseSignal = ReadableBaseSignal<T> and requires(SignalReadPort& port)
    {
        std::remove_reference_t<T>{ port };
    };

    template<BaseSignal T>
    struct is_signal<T> : std::true_type
    {};

    //// dynamic container like std vector, map, list, deque .... of signals 

    namespace internal
    {
        template<typename T>
        concept DynamicContainer = not BaseSignal<T> and requires(T v, typename T::value_type e)
        {
            *v.begin();
            v.end();
            v.size();
            v.insert(v.end(), e);
        };
    }

    template<internal::DynamicContainer T>
    struct is_signal<T, std::enable_if_t<is_signal<typename T::value_type>::value>> : std::true_type
    {};

    template<typename T>
    concept ContainerSignal = internal::DynamicContainer<T> and is_signal<T>::value;

    //// compound (simple aggregate for signales and metadata)
    // this is the most loose definition of signal as a struc may also contain no signal

    namespace internal
    {
        template<typename T>
        struct is_std_array : std::false_type
        {};

        template<typename T, size_t N>
        struct is_std_array<std::array<T, N>> : std::true_type
        {};
    }

    template<typename T>
    concept CompoundSiganl =
        std::is_class_v<T> and
        std::is_aggregate_v<T> and
        !internal::DynamicContainer<T> and
        !internal::is_std_array<T>::value;

    template<CompoundSiganl T>
    struct is_signal<T> : std::true_type
    {};

    //// static tuple/array. note C arrays are not supported due to simple aggregate limitation

    template<typename T>
    concept TupleSignal = 
        not CompoundSiganl<T> and (
            (internal::is_std_array<T>::value and is_signal<typename T::value_type>::value) or
            (!internal::is_std_array<T>::value and requires(T v) {
                    std::tuple_size<T>::value;
                }
            )
        );

    template<TupleSignal T>
    struct is_signal<T> : std::true_type
    {};

    ////

    template<typename T>
    concept Signal =
        BaseSignal<T> or
        CompoundSiganl<T> or
        ContainerSignal<T> or
        TupleSignal<T>;


    template<typename SigA, typename SigB>
    inline NormalizedWidthOperands::NormalizedWidthOperands(const SigA& l, const SigB& r)
    {
        lhs = l.getReadPort();
        rhs = r.getReadPort();

        const size_t maxWidth = std::max(width(lhs), width(rhs));

        hlim::ConnectionType::Interpretation type = hlim::ConnectionType::BITVEC;
        if(maxWidth == 1 &&
            (l.getConnType().interpretation != r.getConnType().interpretation ||
            l.getConnType().interpretation == hlim::ConnectionType::BOOL))
            type = hlim::ConnectionType::BOOL;

        lhs = lhs.expand(maxWidth, type);
        rhs = rhs.expand(maxWidth, type);
    }


}
