#pragma once

#if defined(__GNUC__) && ( __GNUC__ == 12 || __GNUC__ == 13 ) 


/*
	GCC 12.1 - 13.2 has a bug that triggers an internal compiler error for the following code, which gets triggered through the boost::pfr::structure_tie of any struct that contains an array of a type with a virtual destructor.

			#include <array>

			class A
			{
				public:
					virtual ~A();
			};


			template<std::size_t N>
			struct ForceNDeduction { };

			template <class T, std::size_t N>
			constexpr auto like_boost_pfr_detect_fields_count_dispatch(ForceNDeduction<N>)
				-> decltype(sizeof(T{}))
			{
				return 0;
			}


			void forceInstantiation()
			{
				using T = std::array<A, 1>;

				using thisIsOk = decltype(sizeof(T{}));

				auto a = like_boost_pfr_detect_fields_count_dispatch<T>(ForceNDeduction<42>{});
			}

	To circumvent this, we provide a specialized code for std::array that circumvents the boost::pfr::structure_tie of the array:
*/ 

#define detect_fields_count_dispatch detect_fields_count_dispatch___
#define fields_count fields_count___

#include <boost/pfr/detail/fields_count.hpp>

#undef detect_fields_count_dispatch
#undef fields_count


// Taken from boost with modifications:

namespace boost::pfr::detail {

///////////////////// Choosing between array size, greedy and non greedy search.
template <class T, std::size_t N>
constexpr auto detect_fields_count_dispatch(size_t_<N>, long, long) noexcept
    -> typename std::enable_if<std::is_array<T>::value, std::size_t>::type
{
    return sizeof(T) / sizeof(typename std::remove_all_extents<T>::type);
}

template <class T, std::size_t N>
constexpr auto detect_fields_count_dispatch(size_t_<N>, long, int) noexcept
    -> decltype(sizeof(T)) // ======================================= Must not be decltype(sizeof(T{})) for it to not trigger the bug =======================================
{
    constexpr std::size_t middle = N / 2 + 1;
    return detail::detect_fields_count<T, 0, middle>(detail::multi_element_range{}, 1L);
}

template <class T, std::size_t N>
constexpr std::size_t detect_fields_count_dispatch(size_t_<N>, int, int) noexcept {
    // T is not default aggregate initialzable. It means that at least one of the members is not default constructible,
    // so we have to check all the aggregate initializations for T up to N parameters and return the bigest succeeded
    // (we can not use binary search for detecting fields count).
    return detail::detect_fields_count_greedy<T, 0, N>(detail::multi_element_range{});
}


// Override to use the fixed detect_fields_count_dispatch:

template <class T>
constexpr std::size_t fields_count() noexcept {
    using type = std::remove_cv_t<T>;

    static_assert(
        !std::is_reference<type>::value,
        "====================> Boost.PFR: Attempt to get fields count on a reference. This is not allowed because that could hide an issue and different library users expect different behavior in that case."
    );

#if !BOOST_PFR_HAS_GUARANTEED_COPY_ELISION
    static_assert(
        std::is_copy_constructible<std::remove_all_extents_t<type>>::value || (
            std::is_move_constructible<std::remove_all_extents_t<type>>::value
            && std::is_move_assignable<std::remove_all_extents_t<type>>::value
        ),
        "====================> Boost.PFR: Type and each field in the type must be copy constructible (or move constructible and move assignable)."
    );
#endif  // #if !BOOST_PFR_HAS_GUARANTEED_COPY_ELISION

    static_assert(
        !std::is_polymorphic<type>::value,
        "====================> Boost.PFR: Type must have no virtual function, because otherwise it is not aggregate initializable."
    );

#ifdef __cpp_lib_is_aggregate
    static_assert(
        std::is_aggregate<type>::value             // Does not return `true` for built-in types.
        || std::is_scalar<type>::value,
        "====================> Boost.PFR: Type must be aggregate initializable."
    );
#endif


    constexpr std::size_t max_fields_count = (sizeof(type) * CHAR_BIT); // We multiply by CHAR_BIT because the type may have bitfields in T

    constexpr std::size_t result = detail::detect_fields_count_dispatch<type>(size_t_<max_fields_count>{}, 1L, 1L);

    detail::assert_first_not_base<type>(detail::make_index_sequence<result>{});

#ifndef __cpp_lib_is_aggregate
    static_assert(
        is_aggregate_initializable_n<type, result>::value,
        "====================> Boost.PFR: Types with user specified constructors (non-aggregate initializable types) are not supported."
    );
#endif

    static_assert(
        result != 0 || std::is_empty<type>::value || std::is_fundamental<type>::value || std::is_reference<type>::value,
        "====================> Boost.PFR: If there's no other failed static asserts then something went wrong. Please report this issue to the github along with the structure you're reflecting."
    );

    return result;
}

} // namespace boost::pfr::detail

#endif

#include <boost/pfr.hpp>
