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

#include "TechnologyCapabilities.h"

#include "../Scope.h"

namespace gtry {


MemoryCapabilities::Choice MemoryCapabilities::select(const Request &request) const
{
	return select(GroupScope::getCurrentNodeGroup(), request);
}

MemoryCapabilities::Choice MemoryCapabilities::select(hlim::NodeGroup *group, const Request &request) const
{
	/*
	* Default assumption:
	*   - No LARGE memory
	*   - No external memory
	*   - 64 words max for SMALL
	*   - SMALL is zero read cycle latency
	*   - MEDIUM is one read cycle latency
	*/

	MemoryCapabilities::Choice result;
	if ((request.maxDepth <= 64 && request.sizeCategory.contains(SizeCategory::SMALL)) || request.sizeCategory == SizeCategory::SMALL) {
		// SizeCategory::SMALL;
		result.inputRegs = false;
		result.outputRegs = 0;
		result.totalReadLatency = 0;
	} else if (request.sizeCategory.contains(SizeCategory::MEDIUM)) {
		// SizeCategory::MEDIUM;
		result.inputRegs = true;
		result.outputRegs = 0;
		result.totalReadLatency = 1;
	} else
		HCL_DESIGNCHECK_HINT(false, "No suitable memory configuration could be found. The default technology capabilities are limited to SMALL and MEDIUM memories!");

	return result;
}


FifoCapabilities::Choice FifoCapabilities::select(const Request &request) const
{
	return select(GroupScope::getCurrentNodeGroup(), request);
}

FifoCapabilities::Choice FifoCapabilities::select(hlim::NodeGroup *group, const Request &request) const
{
	/*
	* Default assumption:
	*   - Build in configurable logic, so any pow2 size possible
	*/
	Choice choice;

	HCL_ASSERT_HINT(request.readWidth.choice == Preference::SPECIFIC_VALUE, "Read width must be specifc value!");
	HCL_ASSERT_HINT(request.writeWidth.choice == Preference::SPECIFIC_VALUE, "Write width must be specifc value!");

	choice.readWidth = request.readWidth.value;
	choice.writeWidth = request.writeWidth.value;

	switch (request.readDepth.choice) {
		case Preference::MIN_VALUE: 
			choice.readDepth = utils::nextPow2(request.readDepth.value);
		break;
		case Preference::MAX_VALUE:
			choice.readDepth = 1ull << utils::Log2(request.readDepth.value);
		break;
		case Preference::SPECIFIC_VALUE:
			choice.readDepth = request.readDepth.value;
		break;
		default:
			choice.readDepth = 32;
	}

	choice.singleClock = request.singleClock.resolveSimpleDefault(true);

	choice.latency_writeToEmpty = request.latency_writeToEmpty.resolveToPreferredMinimum(2);
	choice.latency_readToFull = request.latency_readToFull.resolveToPreferredMinimum(2);
	choice.latency_writeToAlmostEmpty = request.latency_writeToAlmostEmpty.resolveToPreferredMinimum(2);
	choice.latency_readToAlmostFull = request.latency_readToAlmostFull.resolveToPreferredMinimum(2);

	return choice;
}



}