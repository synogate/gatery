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

namespace gtry::utils {
        
    ///@todo: Use Concepts!!!    


    template<typename Type, typename = void>
    struct isContainer : std::false_type { };

    template<typename Container>
    struct isContainer<
                Container, 
                std::void_t<
                    decltype(std::declval<Container&>().begin()),
                    decltype(std::declval<Container&>().end())
                    >
            > : std::true_type { };
                

            
    template<typename Type, typename = void>
    struct isSignal : std::false_type { };

    template<typename Type>
    struct isSignal<Type, typename Type::isSignal> : std::true_type { };


    template<typename Type, typename = void>
    struct isElementarySignal : std::false_type { };

    template<typename Type>
    struct isElementarySignal<Type, typename Type::isElementarySignal> : std::true_type { };


    template<typename Type, typename = void>
    struct isBitSignal : std::false_type { };

    template<typename Type>
    struct isBitSignal<Type, typename Type::isBitSignal> : std::true_type { };



    template<typename Type, typename = void>
    struct isBitVectorSignal : std::false_type { };

    template<typename Type>
    struct isBitVectorSignal<Type, typename Type::isBitVectorSignal> : std::true_type { };

    
    template<typename Type, typename = void>
    struct isBitVectorSignalLikeOnly : std::false_type { };

    template<typename Type>
    struct isBitVectorSignalLikeOnly<Type, typename Type::isBitVectorSignalLike> : std::true_type { };
    
    template<typename Type, typename = void>
    struct isBitVectorSignalLike : std::false_type { };

    template<typename Type>
    struct isBitVectorSignalLike<Type, typename std::enable_if_t<isBitVectorSignal<Type>::value || isBitVectorSignalLikeOnly<Type>::value>> : std::true_type { };


}

namespace gtry {

	template<typename T>
	struct isCString : std::false_type { };

	template<unsigned S>
	struct isCString<char[S]> : std::true_type { };

	template<typename T>
	concept CString = isCString<T>::value;


    class Bit;
	template<typename T>
	concept BitLiteral = std::same_as<T, char> || std::same_as<T, bool>;

	template<typename T>
	concept BitValue = std::same_as<Bit, T> || BitLiteral<T>;


    class BaseBitVector;
    template<typename T>
    concept BitVectorDerived = std::is_base_of<BaseBitVector, T>::value;



	class BVec;
	template<typename T>
	concept BVecIntegralLiteral = false;

	template<typename T>
	concept BVecLiteral = false;

	template<typename T>
	concept BVecValue = std::same_as<BVec, T> || BVecLiteral<T>;



	class UInt;
	template<typename T>
	concept UIntIntegralLiteral = std::integral<T> && !std::same_as<T, char> && !std::same_as<T, bool>;

	template<typename T>
	concept UIntLiteral = UIntIntegralLiteral<T> || CString<T>;

	template<typename T>
	concept UIntValue = std::same_as<UInt, T> || UIntLiteral<T>;


	class SInt;
	template<typename T>
	concept SIntIntegralLiteral = false;

	template<typename T>
	concept SIntLiteral = false;

	template<typename T>
	concept SIntValue = std::same_as<SInt, T> || SIntLiteral<T>;


    template<typename T>
    concept ArithmeticSignal = std::same_as<SInt, T> || std::same_as<UInt, T>;



    template<typename T>
    concept BitVectorLiteral = BVecLiteral<T> || UIntLiteral<T> || SIntLiteral<T>;

    template<typename T>
    concept BitVectorValue = BVecValue<T> || UIntValue<T> || SIntValue<T>;



    template<typename Type>
    struct is_signal : std::false_type { using sig_type = Type; };

    template<BitValue Type>
    struct is_signal<Type> : std::true_type { using sig_type = Bit; };

    template<BVecValue Type>
    struct is_signal<Type> : std::true_type { using sig_type = BVec; };

    template<UIntValue Type>
    struct is_signal<Type> : std::true_type { using sig_type = UInt; };

    template<SIntValue Type>
    struct is_signal<Type> : std::true_type { using sig_type = SInt; };


    template<typename Type>
    concept signal_convertible = is_signal<Type>::value;



    template <typename T, typename = int>
    struct resizable : std::false_type {};

    template <typename T>
    struct resizable <T, decltype((void)std::declval<T>().resize(1), 0)> : std::true_type {};


}
