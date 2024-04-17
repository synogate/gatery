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
#include "ClangTupleWorkaround.h"

namespace gtry 
{
	struct construct_from_t {};

/**
 * @addtogroup gtry_frontend
 * @{
 */
	template<typename T> requires (!Signal<T>)
	auto constructFrom(const T& val) { return val; }

	template<ContainerSignal T>
	T constructFrom(const T& val);

	template<ReverseSignal T>
	T constructFrom(const T& val);

	template<TupleSignal T>
	auto constructFrom(const T& val);

	template<BaseSignal T>
	T constructFrom(const T& val)
	{
		return T{ val, construct_from_t{} };
	}

	template<CompoundSignal T>
	T constructFrom(const T& val)
	{
		return gtry::make_from_tuple<T>(
			boost::hana::transform(structure_tie(val), [&](auto&& member) {
				return constructFrom(member); // Needed for everything to allow overrides for meta signals as well.
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

	template<ReverseSignal T>
	T constructFrom(const T& val)
	{
		return constructFrom(*val);
	}

	template<TupleSignal T>
	auto constructFrom(const T& val)
	{
		using namespace internal;

		return std::apply([](auto&&... args) {
			return T{ std::forward<std::remove_cvref_t<decltype(args)>>(constructFrom(args))...};
		}, val);
	}

	template<Signal T>
	T dontCare(const T& blueprint)
	{
		// TODO: we should add some kind of read dont care to 
		//		 ignore conditional scopes on first assignment.

		T ret = constructFrom(blueprint);
		unpack(ConstUInt(width(ret)), ret);
		return ret;
	}

	template<Signal T>
	T allZeros(const T& blueprint)
	{
		T ret = constructFrom(blueprint);
		unpack(ConstUInt(0, width(ret)), ret);
		return ret;
	}

	template<Signal T>
	T undefined(const T& blueprint)
	{
		T ret = constructFrom(blueprint);
		unpack(ConstUInt(width(ret)), ret);
		return ret;
	}
/// @}	
}
