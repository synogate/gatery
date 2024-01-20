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
#include "SimSigHandle.h"

#include <gatery/simulation/SimulationContext.h>
#include <gatery/simulation/Simulator.h>
#include <gatery/hlim/ClockRational.h>

#include "Clock.h"

namespace gtry {

SigHandleBit simu(const InputPin &pin)
{
	return SigHandleBit({.node=pin.node(), .port=0ull});
}

SigHandleBVec simu(const InputPins &pins)
{
	return SigHandleBVec({.node=pins.node(), .port=0ull});
}

SigHandleBit simu(const OutputPin &pin)
{
	auto driver = pin.node()->getDriver(0);
	HCL_DESIGNCHECK_HINT(driver.node != nullptr, "Can't read unbound output pin!");
	return SigHandleBit(driver);
}

SigHandleBVec simu(const OutputPins &pins)
{
	auto driver = pins.node()->getDriver(0);
	HCL_DESIGNCHECK_HINT(driver.node != nullptr, "Can't read unbound output pin!");
	return SigHandleBVec(driver);
}


sim::WaitClock AfterClk(const Clock &clk)
{
	return sim::WaitClock(clk.getClk(), sim::WaitClock::AFTER);
}

sim::WaitClock OnClk(const Clock &clk)
{
	return sim::WaitClock(clk.getClk(), sim::WaitClock::DURING);
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

	auto shift = (unsigned) std::abs(cycles) / clk.absoluteFrequency();

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

	auto shift = (unsigned) std::abs(cycles) / clk.absoluteFrequency();

	if (cycles > 0)
		sim->annotationEnd(sim->getCurrentSimulationTime() + shift, id);
	else {
		HCL_DESIGNCHECK_HINT(sim->getCurrentSimulationTime() > shift, "Attempting to set annotation start to before simulation start!");
		sim->annotationEnd(sim->getCurrentSimulationTime() - shift, id);
	}
}

Seconds getCurrentSimulationTime()
{
	auto *sim = sim::SimulationContext::current()->getSimulator();
	HCL_DESIGNCHECK_HINT(sim, "Can only get current simulation time if running in a simulation!");

	return sim->getCurrentSimulationTime();
}

bool simulationIsShuttingDown()
{
	auto *sim = sim::SimulationContext::current()->getSimulator();
	HCL_DESIGNCHECK_HINT(sim, "Can only get simulationIsShuttingDown if running in a simulation!");

	return sim->simulationIsShuttingDown();
}

bool simHasData(std::string_view key)
{
	return sim::SimulationContext::current()->hasAuxData(key);
}

std::any& registerSimData(std::string_view key, std::any data)
{
	return sim::SimulationContext::current()->registerAuxData(key, std::move(data));
}

std::any& getSimData(std::string_view key)
{
	return sim::SimulationContext::current()->getAuxData(key);
}

}
