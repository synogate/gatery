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

#include <type_traits>
#include <concepts>
#include <string_view>


namespace gtry {

    /***************************************/
    /*  General helper concepts and traits */
    /***************************************/

	template<typename T>
	struct isCString : std::false_type { };

	template<unsigned S>
	struct isCString<char[S]> : std::true_type { };

	template<typename T>
	concept CString = isCString<T>::value;



    template <typename T, typename = int>
    struct resizable : std::false_type {};

    template <typename T>
    struct resizable <T, decltype((void)std::declval<T>().resize(1), 0)> : std::true_type {};




    /***************************************/
    /*  Signal related concepts and traits */
    /***************************************/


    class SignalReadPort;

    template<typename T, typename = void>
    struct is_signal : std::false_type
    {};
    
    //// Base signal (any of Bit, BVec, UInt, SInt, ...)

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


    // Base other concepts on BaseSignal even though its not needed.
    // This is to make the Bit/BVec/... concepts more specific and
    // higher priority than BaseSignal in case of ambiguous overloading.

    // General concepts.
    // Make these struct/trait derived to allow outside extensions


    template<typename T>
    struct is_arithmetic_value : std::false_type { };
    /// @brief Signals that allow basic arithmetic (+, -)
    template<typename T>
    concept ArithmeticValue = is_arithmetic_value<T>::value;
    template<typename T>
    concept ArithmeticSignal = BaseSignal<T> && is_arithmetic_value<T>::value;


    template<typename T>
    struct is_bitvector_literal : std::false_type { };
    /// @brief Literals that can be converted into any bitvector based signals (UInt, SInt, ...)
    template<typename T>
    concept BitVectorLiteral = is_bitvector_literal<T>::value;


    template<typename T>
    struct is_bitvector_value : std::false_type { };
    /// @brief Any bitvector based signals (UInt, SInt, ...) or literals for them
    template<typename T>
    concept BitVectorValue = is_bitvector_value<T>::value;


    template<typename Type>
    struct is_base_signal_value : std::false_type { using sig_type = Type; };
    /// @brief Any signals (Bit, UInt, SInt, ...) or literals for them
    template<typename Type>
    concept BaseSignalValue = is_base_signal_value<Type>::value;
    /// @brief Converts any signal value type (signal or literal) to the corresponding signal type
    template<typename Type>
    using ValueToBaseSignal = is_base_signal_value<Type>::sig_type;


    //// Bit

    class Bit;
    /// @brief Any type that can be converted into a Bit
	template<typename T>
	concept BitLiteral = std::same_as<T, char> || std::same_as<T, bool>;

    /// @brief Any type that is or can be converted into a Bit
	template<typename T>
	concept BitValue = (BaseSignal<T> && std::same_as<Bit, T>) || BitLiteral<T>;

    template<BitValue T> struct is_base_signal_value<T> : std::true_type { using sig_type = Bit; };


    //// BaseBitVector derived

    class BaseBitVector;
    template<typename T>
    concept BitVectorDerived = BaseSignal<T> && std::is_base_of<BaseBitVector, T>::value;


    //// BVec

	class BVec;
    /// @brief Any integral type that can be converted into a BVec
	template<typename T>
	concept BVecIntegralLiteral = false;

    /// @brief Any type that can be converted into a BVec
	template<typename T>
	concept BVecLiteral = false;

    /// @brief Any type that is or can be converted into a BVec
	template<typename T>
	concept BVecValue = (BitVectorDerived<T> && std::same_as<BVec, T>) || BVecLiteral<T>;


    template<BVecLiteral T> struct is_bitvector_literal<T> : std::true_type { };
    template<BVecValue T>   struct is_bitvector_value<T> : std::true_type { };
    template<BVecValue T>   struct is_base_signal_value<T> : std::true_type { using sig_type = BVec; };


    //// UInt

	class UInt;
    /// @brief Any integral type that can be converted into a UInt
	template<typename T>
	concept UIntIntegralLiteral = std::integral<T> && !std::same_as<T, char> && !std::same_as<T, bool>;

    /// @brief Any type that can be converted into a UInt
	template<typename T>
	concept UIntLiteral = UIntIntegralLiteral<T> || CString<T>;

    /// @brief Any type that is or can be converted into a UInt
	template<typename T>
	concept UIntValue = (BitVectorDerived<T> && std::same_as<UInt, T>) || UIntLiteral<T>;


    template<UIntLiteral T> struct is_bitvector_literal<T> : std::true_type { };
    template<UIntValue T>   struct is_bitvector_value<T> : std::true_type { };
    template<UIntValue T>   struct is_arithmetic_value<T> : std::true_type { };
    template<UIntValue T>   struct is_base_signal_value<T> : std::true_type { using sig_type = UInt; };

    //// SInt

	class SInt;
    /// @brief Any integral type that can be converted into a SInt
	template<typename T>
	concept SIntIntegralLiteral = false;

    /// @brief Any type that can be converted into a SInt
	template<typename T>
	concept SIntLiteral = false;

    /// @brief Any type that is or can be converted into a SInt
	template<typename T>
	concept SIntValue = (BitVectorDerived<T> && std::same_as<SInt, T>) || SIntLiteral<T>;

    template<SIntLiteral T> struct is_bitvector_literal<T> : std::true_type { };
    template<SIntValue T>   struct is_bitvector_value<T> : std::true_type { };
    template<SIntValue T>   struct is_arithmetic_value<T> : std::true_type { };
    template<SIntValue T>   struct is_base_signal_value<T> : std::true_type { using sig_type = SInt; };



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
    concept CompoundSignal =
        std::is_class_v<T> and
        std::is_aggregate_v<T> and
        !internal::DynamicContainer<T> and
        !internal::is_std_array<T>::value;

    template<CompoundSignal T>
    struct is_signal<T> : std::true_type
    {};

    //// static tuple/array. note C arrays are not supported due to simple aggregate limitation

    template<typename T>
    concept TupleSignal = 
        not CompoundSignal<T> and (
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
        CompoundSignal<T> or
        ContainerSignal<T> or
        TupleSignal<T>;


}
