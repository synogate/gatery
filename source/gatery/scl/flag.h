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
#include <gatery/frontend.h>

namespace gtry::scl
{
	/**
	* @brief this function implements a flag where both set and reset are registered
	*/
	Bit flag(const Bit& set, const Bit& reset, char resetValue = '0');
	/**
	* @brief this function implements a flag where set is combinational and reset is registered
	*/
	Bit flagInstantSet(const Bit& set, const Bit& reset, char resetValue = '0');
	/**
	* @brief This function implements a flag where reset is combinational and set is registered.
	*  It is equivalent to !flagInstantSet(reset, set, !resetValue);
	*/
	Bit flagInstantReset(const Bit& set, const Bit& reset, char resetValue = '0');

	inline Bit edge(const Bit& in) { return in != reg(in, '0'); }
	inline Bit edgeRising(const Bit& in) { return in & !reg(in, '1'); }
	inline Bit edgeFalling(const Bit& in) { return !in & reg(in, '0'); }

	/**
	 * @brief combinatorically captures the in value when condition is high and holds it until the next condition.
	*/
	template<Signal T> T capture(const T& in, Bit condition);
	template<Signal T> T capture(const T& in, const T& resetValue, Bit condition);


	/**
	 * @brief Whenever input is high it is forwarded to out and extended by cycles additional cycles.
	 */
	Bit pulseExtend(Bit input, UInt cycles, Bit reset = '0');
}

namespace gtry::scl
{
	template<Signal T>
	T capture(const T& in, Bit condition)
	{
		T value = constructFrom(in);
		value = reg(value);
		IF(condition)
			value = in;
		return final(value);
	}

	template<Signal T>
	T capture(const T& in, const T& resetValue, Bit condition)
	{
		T value = constructFrom(in);
		value = reg(value, resetValue);
		IF(condition)
			value = in;
		return final(value);
	}
}
