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


#include <optional>


#ifdef __clang__
template <typename T>
auto operator<=>(const std::optional<T> &lhs, const std::optional<T> &rhs) {
	bool leftExists = (bool) lhs;
	bool rightExists = (bool) rhs;
	if (auto __c = _VSTD::__synth_three_way(leftExists, rightExists); __c != 0)
		return __c;
	if (leftExists && rightExists)
		return _VSTD::__synth_three_way(*lhs, *rhs);
	return _VSTD::__synth_three_way(0, 0);
}
#endif

