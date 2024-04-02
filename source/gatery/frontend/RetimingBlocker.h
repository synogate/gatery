/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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
#include "Compound.h"


namespace gtry
{

	namespace internal
	{
		SignalReadPort retimingBlocker(SignalReadPort val);
	}

/**
 * @addtogroup gtry_frontend
 * @{
 */

	/// Adds a retiming blocker such that forward register retiming will not attempt to pull a register from this signal.
	template<BaseSignal T>
	T retimingBlocker(const T& val) { return internal::retimingBlocker(val.readPort()); }

	/// Adds a retiming blocker such that forward register retiming will not attempt to pull a register from this signal.
	template<Signal T>
	T retimingBlocker(const T& val) {
		return internal::transformSignal(val, [&](const auto& sig) {
			if constexpr (!Signal<decltype(sig)>)
				return sig;
			else
				return retimingBlocker(sig); // forward so it can have overloads
		});
	}


/// @}

}
