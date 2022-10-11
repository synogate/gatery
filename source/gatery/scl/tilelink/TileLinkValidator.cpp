/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include "TileLinkValidator.h"
#include <gatery/simulation/Simulator.h>

#include <sstream>

namespace gtry::scl
{


	template<StreamSignal T> 
	bool readyDefined(const T &stream) { return true; }

	template<StreamSignal T> requires (T::template has<Ready>())
	bool readyDefined(const T &stream) {  return simu(ready(stream)).allDefined(); }

	template<StreamSignal T> 
	bool validDefined(const T &stream) { return true; }

	template<StreamSignal T> requires (T::template has<Valid>())
	bool validDefined(const T &stream) {  return simu(valid(stream)).allDefined(); }

	template<StreamSignal T>
	SimProcess validateStreamReadyValid(T &stream, const Clock &clk)
	{
		while (true) {
			if (!validDefined(stream)) {
				hlim::BaseNode *node = valid(stream).readPort().node;
				std::stringstream msg;
				msg << "Stream has undefined valid signal at " << hlim::toNanoseconds(sim::SimulationContext::current()->getSimulator()->getCurrentSimulationTime()) << " ns.";// Node from " << node->getStackTrace();
				sim::SimulationContext::current()->onAssert(node, msg.str());
			}
			if (!readyDefined(stream)) {
				hlim::BaseNode *node = ready(stream).readPort().node;
				std::stringstream msg;
				msg << "Stream has undefined ready signal at " << hlim::toNanoseconds(sim::SimulationContext::current()->getSimulator()->getCurrentSimulationTime()) << " ns.";// Node from " << node->getStackTrace();
				sim::SimulationContext::current()->onAssert(node, msg.str());
			}
			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLink(TileLinkChannelA &channelA, TileLinkChannelD &channelD, const Clock &clk)
	{
		co_await fork(validateStreamReadyValid(channelA, clk));
		co_await fork(validateStreamReadyValid(channelD, clk));
	}

}

