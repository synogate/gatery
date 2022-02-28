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
#include "Signal.h"

#include "Bit.h"

#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/zip.hpp>
#include <boost/pfr.hpp>

namespace gtry
{
	class Clock;

	struct RegisterSettings {
		boost::optional<Clock&> clock;
		bool allowRetimingBackward = false;
		bool allowRetimingForward = false;
	};

	template<BaseSignal T>
	T reg(const T& val, const RegisterSettings& settings = {});

	template<BaseSignal T, std::convertible_to<T> Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings = {});

	template<CompoundSiganl T>
	T reg(const T& val, const RegisterSettings& settings = {});

	template<CompoundSiganl T, std::convertible_to<T> Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings = {});

	template<ContainerSignal T>
	T reg(const T& val, const RegisterSettings& settings = {});

	template<ContainerSignal T, std::convertible_to<T> Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings = {});

	template<ArraySignal T>
	T reg(const T& val, const RegisterSettings& settings = {});

	template<ArraySignal T, std::convertible_to<typename T::value_type> Tr, size_t N>
	T reg(const T& val, const std::array<Tr, N>& resetVal, const RegisterSettings& settings = {});

	// implementation
	namespace internal
	{
		SignalReadPort reg(SignalReadPort val, std::string_view name, std::optional<SignalReadPort> reset, const RegisterSettings& settings);

		template<typename T>
		T operator + (const T& v, const RegisterSettings& s) { return reg(v, s); }

		template<typename T>
		T operator + (std::tuple<const T&, const T&> v, const RegisterSettings& s) { return reg(get<0>(v), get<1>(v), s); }
	}
	

	template<BaseSignal T>
	T reg(const T& val, const RegisterSettings& settings)
	{
		return internal::reg(
			val.getReadPort(),
			val.getName(),
			std::nullopt,
			settings
		);
	}

	template<BaseSignal T, std::convertible_to<T> Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings)
	{
		NormalizedWidthOperands ops(val, T{ resetVal });

		return internal::reg(
			ops.lhs,
			val.getName(),
			ops.rhs,
			settings
		);
	}

	template<CompoundSiganl T>
	T reg(const T& val, const RegisterSettings& settings)
	{
		return std::make_from_tuple<T>(
			boost::hana::transform(boost::pfr::structure_tie(val), [&](auto&& member) {
				if constexpr(Signal<decltype(member)>)
					return reg(member, settings);
				else
					return member;
			})
		);
	}

    template<CompoundSiganl T, std::convertible_to<T> Tr>
    T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings)
    {
		const T& resetValT = resetVal;
		return std::make_from_tuple<T>(
			boost::hana::transform(
				boost::hana::zip_with([](auto&& a, auto&& b) { return std::tie(a, b); },
					boost::pfr::structure_tie(val),
					boost::pfr::structure_tie(resetValT)
				), [&](auto member) {

					if constexpr(Signal<decltype(get<0>(member))>)
						return reg(get<0>(member), get<1>(member), settings);
					else
						return get<1>(member); // use reset value for metadata

			})
		);
	}

	template<ContainerSignal T>
	T reg(const T& val, const RegisterSettings& settings)
	{
		T ret;

		if constexpr (requires { ret.reserve(val.size()); })
			ret.reserve(val.size());

		for(auto&& it : val)
			ret.push_back(reg(it, settings));

		return ret;
	}

	template<ContainerSignal T, std::convertible_to<T> Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings)
	{
		HCL_DESIGNCHECK(val.size() == resetVal.size());

		T ret;

		if constexpr(requires { ret.reserve(val.size()); })
			ret.reserve(val.size());

		const T& resetValT = resetVal;
		auto it_reset = resetValT.begin();
		for(auto&& it : val)
			ret.push_back(reg(it, *it_reset++, settings));

		return ret;
	}

	template<ArraySignal T>
	T reg(const T& val, const RegisterSettings& settings)
	{
		using namespace internal;

		auto in2 = boost::hana::unpack(val, [](auto&&... args) {
			return std::tie(args...);
		});

		return boost::hana::unpack(in2, [&](auto&&... args) {
			return T{ { (args + settings)... } };
		});
	}

	template<ArraySignal T, std::convertible_to<typename T::value_type> Tr, size_t N>
	T reg(const T& val, const std::array<Tr, N>& resetVal, const RegisterSettings& settings)
	{
		using namespace internal;

		auto in2 = boost::hana::unpack(val, [](auto&&... args) {
			return std::tie(args...);
		});
		auto in2r = boost::hana::unpack(resetVal, [](auto&&... args) {
			return std::tie(args...);
		});

		auto in3 = boost::hana::zip_with(std::tie<const typename T::value_type&, const typename T::value_type&>, in2, in2r);

		return boost::hana::unpack(in3, [&](auto&&... args) {
			return T{ { (args + settings)... } };
		});
	}


}
