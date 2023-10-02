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

#include "gatery/pch.h"

#include "Attributes.h"

#include "Signal.h"
#include "Clock.h"

#include "DesignScope.h"
#include "../hlim/supportNodes/Node_PathAttributes.h"

namespace gtry {

void attribute(ElementarySignal &signal, SignalAttributes attributes)
{
	signal.attribute(std::move(attributes));
}

void pathAttribute(ElementarySignal &start, ElementarySignal &end, PathAttributes attributes)
{
	auto* node = DesignScope::createNode<hlim::Node_PathAttributes>();
	node->getAttribs() = std::move(attributes);
	node->connectStart(start.readPort());
	node->connectEnd(end.readPort());
}

SignalReadPort internal::allowClockDomainCrossing(const ElementarySignal& in, const Clock &srcClock, const Clock &dstClock, hlim::Node_CDC::CdcNodeParameter params)
{
	auto* node = DesignScope::createNode<hlim::Node_CDC>();
	node->attachClock(srcClock.getClk(), (size_t)hlim::Node_CDC::Clocks::INPUT_CLOCK);
	node->attachClock(dstClock.getClk(), (size_t)hlim::Node_CDC::Clocks::OUTPUT_CLOCK);
	node->connectInput(in.readPort());
	node->setCdcNodeParameter(params);
	return SignalReadPort( node );
}

}
