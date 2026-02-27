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
#include "Reverse.h"

#include "../hlim/Attributes.h"

#include <gatery/utils/Traits.h>
#include "../hlim/supportNodes/Node_CDC.h"

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


class Clock;

namespace internal {
	/// Inserts a node that allows clock domain crossing and verifies that the crossing happens from/to the specified clocks.
	SignalReadPort allowClockDomainCrossing(const ElementarySignal& in, const Clock &srcClock, const Clock &dstClock, const hlim::Node_CDC::CdcNodeParameter params = {});

	struct AllowCdcReverseScope
	{
		inline static thread_local bool inReverseAllowCdc = false;

		AllowCdcReverseScope() { inReverseAllowCdc = !inReverseAllowCdc; }
		~AllowCdcReverseScope() { inReverseAllowCdc = !inReverseAllowCdc; }
	};
}

/// Inserts a node that allows clock domain crossing and verifies that the crossing happens from/to the specified clocks.
template<BaseSignal SignalType>
std::remove_reference_t<SignalType> allowClockDomainCrossing(SignalType&& in, const Clock &srcClock, const Clock &dstClock, const hlim::Node_CDC::CdcNodeParameter params = {}) {
	if(!internal::AllowCdcReverseScope::inReverseAllowCdc)
		return internal::allowClockDomainCrossing(in, srcClock, dstClock, params);

	std::remove_cvref_t<SignalType> s = constructFrom(in);
	in = internal::allowClockDomainCrossing(s, dstClock, srcClock, params);
	return s;
}
	

/// Inserts a node that allows clock domain crossing and verifies that the crossing happens from/to the specified clocks.
template<Signal T>
std::remove_reference_t<T> allowClockDomainCrossing(T&& val, const Clock &srcClock, const Clock &dstClock, const hlim::Node_CDC::CdcNodeParameter params = {})
{
	return internal::transformSignal(std::forward<T>(val), [&](auto&& sig) {
		if constexpr (!Signal<std::remove_reference_t<decltype(sig)>>)
			return sig;
		else
			return allowClockDomainCrossing(std::forward<decltype(sig)>(sig), srcClock, dstClock, params);
	});
}

// reverse clock direction on reverse signals
template<Signal T>
Reverse<T> allowClockDomainCrossing(Reverse<T>&& val, const Clock& srcClock, const Clock& dstClock, const hlim::Node_CDC::CdcNodeParameter params = {})
{
	internal::AllowCdcReverseScope scope;
	return allowClockDomainCrossing(move(*val), srcClock, dstClock, params);
}

template<Signal T>
T allowClockDomainCrossing(const Reverse<T>& val, const Clock& srcClock, const Clock& dstClock, const hlim::Node_CDC::CdcNodeParameter params = {})
{
	static_assert(!"reverse signals must be rvalue");
}

template<Signal T>
T allowClockDomainCrossing(Reverse<T>& val, const Clock& srcClock, const Clock& dstClock, const hlim::Node_CDC::CdcNodeParameter params = {})
{
	static_assert(!"reverse signals must be rvalue");
}

inline auto allowClockDomainCrossing(const Clock& srcClock, const Clock& dstClock, const hlim::Node_CDC::CdcNodeParameter params = {})
{
	return [&, params](auto&& in) { return allowClockDomainCrossing(std::forward<decltype(in)>(in), srcClock, dstClock, params); };
}


/**@}*/

}

