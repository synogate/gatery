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

#include "../hlim/Attributes.h"

#include <gatery/utils/Traits.h>

namespace gtry {

class ElementarySignal;

using hlim::SignalAttributes;
using hlim::PathAttributes;


/**
 * @addtogroup gtry_frontend
 * @{
 */


namespace internal {

/// Set an attribute for a signal, such as max-fanout or vendor specific attributes.
SignalReadPort attribute(const ElementarySignal &signal, SignalAttributes attributes);

}


/**
 * @brief Set an attribute for a signal, such as max-fanout or vendor specific attributes
 * 
 * @param signal Signal to which to apply the attribute. Often, attributes then actually refer to the driver of this signal.
 * @param attributes The attributes to set, potentially overwrites previous attributes.
 */
template<BaseSignal SignalType>
inline SignalType attribute(const SignalType& in, SignalAttributes attributes) {
	return internal::attribute(in, std::move(attributes));
}
	

/**
 * @brief Set an attribute for a signal, such as max-fanout or vendor specific attributes
 * 
 * @param signal Signal to which to apply the attribute. Often, attributes then actually refer to the driver of this signal.
 * @param attributes The attributes to set, potentially overwrites previous attributes.
 */
template<Signal T>
T attribute(const T& val, SignalAttributes attributes)
{
	return internal::transformSignal(val, [&](const auto& sig) {
		if constexpr (!Signal<decltype(sig)>)
			return sig;
		else
			return attribute(sig, attributes);
	});
}




/**
 * @brief Sets an attribute for a signal path, such as false-path or multi-cycle.
 * @details The path is defined through a start node and an end node.
 * @param start Start node of the path
 * @param end End node of the path
 * @param attributes The attributes to set for this path
 */
void pathAttribute(ElementarySignal &start, ElementarySignal &end, PathAttributes attributes);

/**@}*/

}
