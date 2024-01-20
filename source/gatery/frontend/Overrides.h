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

#include "Compound.h"
#include "Pin.h"

namespace gtry {

	template<BaseSignal T, typename Tr>
	T simOverride(T val, const Tr& simulationSignal)
	{
		val.simulationOverride(simulationSignal);
		return val;
	}

	template<Signal T, typename Tr> requires(!BaseSignal<T>)
	T simOverride(const T& val, const Tr& simulationSignal)
	{
		return internal::transformSignal(val, simulationSignal, [&](const auto& sig, auto&& simulationSignal) {
			if constexpr (!Signal<decltype(sig)>)
				return sig;
			else
				return simOverride(sig, simulationSignal);
		});
	}

	template<Signal T>
	T simOverrideDownstream(T&& in, T&& simulationSignal)
	{
		T ret = constructFrom(in);
		downstream(ret) = simOverride(copy(downstream(in)), downstream(simulationSignal));
		upstream(in) = upstream(ret);
		upstream(simulationSignal) = upstream(ret);

		return ret;
	}

	template<Signal T>
	std::tuple<T, T> simOverrideUpstream(T&& in)
	{
		T ret = constructFrom(in);
		T simulationSignal = constructFrom(in);

		downstream(ret) = downstream(in);
		downstream(simulationSignal) = downstream(in);
		upstream(in) = simOverride(copy(upstream(ret)), upstream(simulationSignal));

		return std::make_tuple(move(ret), move(simulationSignal));
	}


	template<Signal T>
	T simDriveDownstream(T&& in, std::string simPrefix) {
		T simIn = constructFrom(in);
		pinIn(simIn, simPrefix, { .simulationOnlyPin = true });

		in = simOverrideDownstream(move(in), move(simIn));
		return in;
	}
}
