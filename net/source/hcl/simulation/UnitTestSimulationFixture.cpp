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
#include "UnitTestSimulationFixture.h"

#include "ReferenceSimulator.h"

#include <boost/test/unit_test.hpp>

#include "../hlim/Circuit.h"

namespace hcl::core::sim {

UnitTestSimulationFixture::UnitTestSimulationFixture()
{
    m_simulator.reset(new ReferenceSimulator());
    m_simulator->addCallbacks(this);
}

UnitTestSimulationFixture::~UnitTestSimulationFixture()
{
}

void UnitTestSimulationFixture::addSimulationProcess(std::function<SimulationProcess()> simProc)
{
    m_simulator->addSimulationProcess(std::move(simProc));
}


void UnitTestSimulationFixture::eval(hlim::Circuit &circuit)
{
    m_simulator->compileProgram(circuit);
    m_simulator->powerOn();
    //m_simulator->reevaluate();
}

void UnitTestSimulationFixture::runTicks(hlim::Circuit &circuit, const hlim::Clock *clock, unsigned numTicks)
{
    m_runLimClock = 0;
    m_runLimClock = clock;

    m_simulator->compileProgram(circuit);
    m_simulator->powerOn();
    while (m_runLimTicks < numTicks)
        m_simulator->advanceEvent();
}


void UnitTestSimulationFixture::onNewTick(const hlim::ClockRational &simulationTime)
{
}

void UnitTestSimulationFixture::onClock(const hlim::Clock *clock, bool risingEdge)
{
    if (clock == m_runLimClock && risingEdge)
        m_runLimTicks++;
}

void UnitTestSimulationFixture::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
    BOOST_TEST_MESSAGE(msg);
}

void UnitTestSimulationFixture::onWarning(const hlim::BaseNode *src, std::string msg)
{
    BOOST_ERROR(msg);
}

void UnitTestSimulationFixture::onAssert(const hlim::BaseNode *src, std::string msg)
{
    BOOST_FAIL(msg);
}

}
