/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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


#include <tuple>

#ifdef __clang__

namespace gtry {
	// Since clang does not support C++20 aggregate initialization with round braces, this builds the exact same mechanism but with curly braces.
	template <typename _Tp, typename _Tuple, std::size_t... _Idx>
	constexpr _Tp __make_from_tuple_impl(_Tuple&& __t, std::index_sequence<_Idx...>)
	{ 
		return _Tp{std::get<_Idx>(std::forward<_Tuple>(__t))...};  // < curly braces on _Tp
	}
#define _LIBCPP_NOEXCEPT_RETURN(...) noexcept(noexcept(__VA_ARGS__)) { return __VA_ARGS__; }
	template <typename _Tp, typename _Tuple>
	constexpr _Tp make_from_tuple(_Tuple&& __t) _LIBCPP_NOEXCEPT_RETURN
    (
		gtry::__make_from_tuple_impl<_Tp>(std::forward<_Tuple>(__t), std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<_Tuple>>>{})
    )
#undef _LIBCPP_NOEXCEPT_RETURN
}

#else

namespace gtry {
	template <typename _Tp, typename _Tuple>
	constexpr _Tp make_from_tuple(_Tuple&& __t)
	{
		return std::make_from_tuple<_Tp>(std::forward<_Tuple>(__t));
	}
}

#endif
