#pragma once

#include "Pin.h"
#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"

#include <hcl/simulation/SigHandle.h>
#include <hcl/simulation/simFiber/SimulationFiber.h>
#include <hcl/simulation/simFiber/WaitFor.h>
#include <hcl/simulation/simFiber/WaitUntil.h>


namespace hcl::core::frontend {

sim::SigHandle sim(hlim::NodePort output);
sim::SigHandle sim(const Bit &bit);
sim::SigHandle sim(const BVec &signal);
sim::SigHandle sim(const InputPin &pin);
sim::SigHandle sim(const InputPins &pins);

using SimFiber = sim::SimulationFiber;
using WaitFor = sim::WaitFor;
using WaitUntil = sim::WaitUntil;
using Seconds = hlim::ClockRational;

}