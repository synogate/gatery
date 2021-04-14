/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "SimSigHandle.h"

#include <gatery/simulation/SimulationContext.h>
#include <gatery/simulation/Simulator.h>
#include <gatery/hlim/ClockRational.h>

#include "Clock.h"

namespace gtry {


hlim::Node_Pin* findInputPin(hlim::NodePort driver)
{
    hlim::Node_Pin* res;
    if (res = dynamic_cast<hlim::Node_Pin*>(driver.node))
        return res;

    HCL_DESIGNCHECK(driver.node != nullptr);
    if (dynamic_cast<hlim::Node_Signal*>(driver.node) == nullptr)
        return nullptr;

    for (auto nh : driver.node->exploreInput(0)) {
        if (res = dynamic_cast<hlim::Node_Pin*>(nh.node()))
            return res;
        else
            if (!nh.isSignal())
                nh.backtrack();
    }
    return nullptr;
}


hlim::Node_Pin *findOutputPin(hlim::NodePort driver)
{
    hlim::Node_Pin *res;
    if (res = dynamic_cast<hlim::Node_Pin*>(driver.node))
        return res;

    HCL_DESIGNCHECK(driver.node != nullptr);

    for (auto nh : driver.node->exploreOutput(driver.port)) {
        if (res = dynamic_cast<hlim::Node_Pin*>(nh.node()))
            return res;
        else
            if (!nh.isSignal())
                nh.backtrack();
    }

    return nullptr;
}

sim::SigHandle simu(hlim::NodePort output)
{
    return sim::SigHandle(output);
}

sim::SigHandle simu(const Bit &bit)
{
    auto driver = bit.getReadPort();
    HCL_DESIGNCHECK(driver.node != nullptr);

    hlim::Node_Pin* pin = findInputPin(driver);
    if (pin)
        return simu({ .node = pin, .port = 0ull });

    pin = findOutputPin(driver);
    if (pin)
        return simu(pin->getDriver(0));

    HCL_DESIGNCHECK_HINT(false, "Found neither input nor output pin associated with signal");
}

sim::SigHandle simu(const BVec &signal)
{
    auto driver = signal.getReadPort();
    HCL_DESIGNCHECK(driver.node != nullptr);

    hlim::Node_Pin* pin = findInputPin(driver);
    if (pin)
        return simu({ .node = pin, .port = 0ull });

    pin = findOutputPin(driver);
    if (pin)
        return simu(pin->getDriver(0));

    HCL_DESIGNCHECK_HINT(false, "Found neither input nor output pin associated with signal");
}

sim::SigHandle simu(const InputPin &pin)
{
    return simu({.node=pin.getNode(), .port=0ull});
}

sim::SigHandle simu(const InputPins &pins)
{
    return simu({.node=pins.getNode(), .port=0ull});
}

sim::SigHandle simu(const OutputPin &pin)
{
    auto driver = pin.getNode()->getDriver(0);
    HCL_DESIGNCHECK_HINT(driver.node != nullptr, "Can't read unbound output pin!");
    return simu(driver);
}

sim::SigHandle simu(const OutputPins &pins)
{
    auto driver = pins.getNode()->getDriver(0);
    HCL_DESIGNCHECK_HINT(driver.node != nullptr, "Can't read unbound output pin!");
    return simu(driver);
}

sim::WaitClock WaitClk(const Clock &clk)
{
    return sim::WaitClock(clk.getClk());
}

void simAnnotationStart(const std::string &id, const std::string &desc)
{
    auto *sim = sim::SimulationContext::current()->getSimulator();
    HCL_DESIGNCHECK_HINT(sim, "Can only annotate if running an actual simulation!");
    sim->annotationStart(sim->getCurrentSimulationTime(), id, desc);
}

void simAnnotationStartDelayed(const std::string &id, const std::string &desc, const Clock &clk, int cycles)
{
    auto *sim = sim::SimulationContext::current()->getSimulator();
    HCL_DESIGNCHECK_HINT(sim, "Can only annotate if running an actual simulation!");

    auto shift = (unsigned) std::abs(cycles) / clk.getAbsoluteFrequency();

    if (cycles > 0)
        sim->annotationStart(sim->getCurrentSimulationTime() + shift, id, desc);
    else {
        HCL_DESIGNCHECK_HINT(sim->getCurrentSimulationTime() > shift, "Attempting to set annotation start to before simulation start!");
        sim->annotationStart(sim->getCurrentSimulationTime() - shift, id, desc);
    }
}

void simAnnotationEnd(const std::string &id)
{
    auto *sim = sim::SimulationContext::current()->getSimulator();
    HCL_DESIGNCHECK_HINT(sim, "Can only annotate if running an actual simulation!");
    sim->annotationEnd(sim->getCurrentSimulationTime(), id);
}

void simAnnotationEndDelayed(const std::string &id, const Clock &clk, int cycles)
{
    auto *sim = sim::SimulationContext::current()->getSimulator();
    HCL_DESIGNCHECK_HINT(sim, "Can only annotate if running an actual simulation!");

    auto shift = (unsigned) std::abs(cycles) / clk.getAbsoluteFrequency();

    if (cycles > 0)
        sim->annotationEnd(sim->getCurrentSimulationTime() + shift, id);
    else {
        HCL_DESIGNCHECK_HINT(sim->getCurrentSimulationTime() > shift, "Attempting to set annotation start to before simulation start!");
        sim->annotationEnd(sim->getCurrentSimulationTime() - shift, id);
    }
}



}
