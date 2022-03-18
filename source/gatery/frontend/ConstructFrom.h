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
#include "Compound.h"

namespace gtry 
{
	template<ContainerSignal T>
	T constructFrom(const T& val);

	template<TupleSignal T>
	T constructFrom(const T& val);

	template<CompoundSignal T>
	T constructFrom(const T& val)
	{
		return std::make_from_tuple<T>(
			boost::hana::transform(boost::pfr::structure_tie(val), [&](auto&& member) {
				if constexpr(Signal<decltype(member)>)
					return constructFrom(member);
				else
					return member;
			})
		);
	}

	template<ContainerSignal T>
	T constructFrom(const T& val)
	{
		T ret;

		if constexpr(requires { ret.reserve(val.size()); })
			ret.reserve(val.size());

		for(auto&& it : val)
			ret.insert(ret.end(), constructFrom(it));

		return ret;
	}

	template<TupleSignal T>
	T constructFrom(const T& val)
	{
		using namespace internal;

		auto in2 = boost::hana::unpack(val, [](auto&&... args) {
			return std::tie(args...);
		});

		return boost::hana::unpack(in2, [&](auto&&... args) {
			return T{ constructFrom(args)... };
		});
	}
}
