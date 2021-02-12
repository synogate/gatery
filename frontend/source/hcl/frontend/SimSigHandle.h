#pragma once

#include "Pin.h"
#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"

#include <hcl/simulation/SigHandle.h>
#include <hcl/simulation/simProc/SimulationProcess.h>
#include <hcl/simulation/simProc/WaitFor.h>
#include <hcl/simulation/simProc/WaitUntil.h>


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

sim::WaitClk waitClk(const Clock &clk);

}