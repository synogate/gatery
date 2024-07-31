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
#include "ConstructFrom.h"

#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/ext/std/pair.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/zip.hpp>
#include <gatery/compat/boost_pfr.h>


namespace gtry
{
/**
 * @addtogroup gtry_frontend
 * @{
 */


	template<BaseSignal T>
	void resetSignal(T& val);

	template<Signal T>
	void resetSignal(T& val);


	template<BaseSignal T>
	void resetSignal(T& val)
	{
		val.resetNode();
	}

	template<Signal T>
	void resetSignal(T& val)
	{
		internal::mutateSignal(val, [&](auto& sig) {
			if constexpr (Signal<decltype(sig)>)
				resetSignal(sig);
		});
	}

/// @}

}
