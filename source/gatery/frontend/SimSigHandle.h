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

#include "Pin.h"
#include "Signal.h"
#include "Bit.h"
#include "UInt.h"

#include <gatery/utils/Traits.h>

#include <gatery/simulation/SigHandle.h>
#include <gatery/simulation/simProc/SimulationProcess.h>
#include <gatery/simulation/simProc/WaitFor.h>
#include <gatery/simulation/simProc/WaitUntil.h>
#include <gatery/simulation/simProc/WaitClock.h>


namespace gtry {

class Clock;

sim::SigHandle simu(hlim::NodePort output);

template<BaseSignal T>
sim::SigHandle simu(const T &signal) {
	auto driver = signal.readPort();
	HCL_DESIGNCHECK(driver.node != nullptr);
	return simu(driver);
}

sim::SigHandle simu(const InputPin &pin);
sim::SigHandle simu(const InputPins &pins);

sim::SigHandle simu(const OutputPin &pin);
sim::SigHandle simu(const OutputPins &pins);

using SimProcess = sim::SimulationProcess;
using WaitFor = sim::WaitFor;
using WaitUntil = sim::WaitUntil;
using Seconds = hlim::ClockRational;

using BigInt = sim::SigHandle::BigInt;

sim::WaitClock WaitClk(const Clock &clk);

void simAnnotationStart(const std::string &id, const std::string &desc);
void simAnnotationStartDelayed(const std::string &id, const std::string &desc, const Clock &clk, int cycles);

void simAnnotationEnd(const std::string &id);
void simAnnotationEndDelayed(const std::string &id, const Clock &clk, int cycles);



}
