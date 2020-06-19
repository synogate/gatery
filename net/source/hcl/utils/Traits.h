#pragma once

#include <type_traits>

namespace hcl::utils {
    
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
struct isUnsignedIntegerSignal : std::false_type { };

template<typename Type>
struct isUnsignedIntegerSignal<Type, typename Type::isUnsignedIntegerSignal> : std::true_type { };



template<typename Type, typename = void>
struct isSignedIntegerSignal : std::false_type { };

template<typename Type>
struct isSignedIntegerSignal<Type, typename Type::isSignedIntegerSignal> : std::true_type { };


template<typename Type, typename = void>
struct isIntegerSignal : std::false_type { };

template<typename Type>
struct isIntegerSignal<Type, std::enable_if_t<
                            isUnsignedIntegerSignal<Type>::value ||
                            isSignedIntegerSignal<Type>::value
                    >> : std::true_type { };



template<typename Type, typename = void>
struct isNumberSignal : std::false_type { };

template<typename Type>
struct isNumberSignal<Type, std::enable_if_t<
                            isIntegerSignal<Type>::value
                            // floats, fixed points, ...
                    >> : std::true_type { };

}
