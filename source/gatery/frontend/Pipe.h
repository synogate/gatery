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
#include "../utils/Traits.h"

namespace gtry 
{
	/**
	 * @brief The pipe operator is an alternative syntax for calling functions. The benefit is to write function calls in the order they are executed.
	 *			It is limited to non-container signals, because iteratables have an overload in the std ranges library.
	*/
	template<Signal SigT> requires (not ContainerSignal<SigT>)
	auto operator | (SigT&& in, std::invocable<SigT&&> auto&& func)
	{
		return std::invoke(std::forward<decltype(func)>(func), std::forward<decltype(in)>(in));
	}
}
