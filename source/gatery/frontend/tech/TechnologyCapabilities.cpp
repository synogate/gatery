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
    if (request.maxDepth <= 64 && request.sizeCategory.contains(SizeCategory::SMALL)) {
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

    choice.readWidth = request.readWidth.value;
    choice.writeWidth = request.writeWidth.value;
    choice.readDepth = request.readDepth.value;

    choice.outputIsFallthrough = request.outputIsFallthrough.value;
    choice.singleClock = request.singleClock.value;
    choice.useECCEncoder = request.useECCEncoder.value;
    choice.useECCDecoder = request.useECCDecoder.value;

    choice.latency_write_empty = request.latency_write_empty.value;
    choice.latency_read_empty = request.latency_read_empty.value;
    choice.latency_write_full = request.latency_write_full.value;
    choice.latency_read_full = request.latency_read_full.value;
    choice.latency_write_almostEmpty = request.latency_write_almostEmpty.value;
    choice.latency_read_almostEmpty = request.latency_read_almostEmpty.value;
    choice.latency_write_almostFull = request.latency_write_almostFull.value;
    choice.latency_read_almostFull = request.latency_read_almostFull.value;

    auto handleSimpleDefault = [](auto &choice, const auto &request, auto defaultValue) {
        HCL_DESIGNCHECK(request.choice != Preference::MIN_VALUE && 
                        request.choice != Preference::MAX_VALUE);
        if (request.choice == Preference::SPECIFIC_VALUE)
            choice = request.value;
        else
            choice = defaultValue;
    };

    auto handlePreferredMinimum = [](size_t &choice, const auto &request, size_t preferredMinimum) {
        switch (request.choice) {
            case Preference::MIN_VALUE: 
                choice = std::max<size_t>(request.value, preferredMinimum);
            break;
            case Preference::MAX_VALUE:
                choice = std::min<size_t>(request.value, preferredMinimum);
            break;
            case Preference::SPECIFIC_VALUE:
                choice = request.value;
            break;
            default:
                choice = preferredMinimum;
        }
    };

    HCL_ASSERT_HINT(request.readWidth.choice == Preference::SPECIFIC_VALUE, "Read width must be specifc value!");
    HCL_ASSERT_HINT(request.writeWidth.choice == Preference::SPECIFIC_VALUE, "Write width must be specifc value!");

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

    handleSimpleDefault(choice.outputIsFallthrough, request.outputIsFallthrough, false);
    handleSimpleDefault(choice.singleClock, request.singleClock, true);
    handleSimpleDefault(choice.useECCEncoder, request.useECCEncoder, false);
    handleSimpleDefault(choice.useECCDecoder, request.useECCDecoder, false);

    handlePreferredMinimum(choice.latency_write_empty,       request.latency_write_empty, 1);
    handlePreferredMinimum(choice.latency_read_empty,        request.latency_read_empty, 1);
    handlePreferredMinimum(choice.latency_write_full,        request.latency_write_full, 1);
    handlePreferredMinimum(choice.latency_read_full,         request.latency_read_full, 1);
    handlePreferredMinimum(choice.latency_write_almostEmpty, request.latency_write_almostEmpty, 1);
    handlePreferredMinimum(choice.latency_read_almostEmpty,  request.latency_read_almostEmpty, 1);
    handlePreferredMinimum(choice.latency_write_almostFull,  request.latency_write_almostFull, 1);
    handlePreferredMinimum(choice.latency_read_almostFull,   request.latency_read_almostFull, 1);

    return choice;
}



}