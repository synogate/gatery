/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

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
#include "BitVector.h"

#include <hcl/simulation/SigHandle.h>
#include <hcl/simulation/simProc/SimulationProcess.h>
#include <hcl/simulation/simProc/WaitFor.h>
#include <hcl/simulation/simProc/WaitUntil.h>
#include <hcl/simulation/simProc/WaitClock.h>


namespace hcl::core::frontend {

class Clock;

sim::SigHandle sim(hlim::NodePort output);
sim::SigHandle sim(const Bit &bit);
sim::SigHandle sim(const BVec &signal);
sim::SigHandle sim(const InputPin &pin);
sim::SigHandle sim(const InputPins &pins);

sim::SigHandle sim(const OutputPin &pin);
sim::SigHandle sim(const OutputPins &pins);

using SimProcess = sim::SimulationProcess;
using WaitFor = sim::WaitFor;
using WaitUntil = sim::WaitUntil;
using Seconds = hlim::ClockRational;

sim::WaitClock WaitClk(const Clock &clk);

void simAnnotationStart(const std::string &id, const std::string &desc);
void simAnnotationStartDelayed(const std::string &id, const std::string &desc, const Clock &clk, int cycles);

void simAnnotationEnd(const std::string &id);
void simAnnotationEndDelayed(const std::string &id, const Clock &clk, int cycles);



}
