/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
