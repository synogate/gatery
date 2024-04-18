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
#include "UnitTestSimulationFixture.h"

#include "ReferenceSimulator.h"

#include <boost/test/unit_test.hpp>

#include "../hlim/Circuit.h"

namespace gtry::sim {

UnitTestSimulationFixture::UnitTestSimulationFixture()
{
	m_simulator.reset(new ReferenceSimulator(false));
	m_simulator->addCallbacks(this);
}

UnitTestSimulationFixture::~UnitTestSimulationFixture()
{
}


void UnitTestSimulationFixture::addSimulationProcess(std::function<SimulationFunction<>()> simProc)
{
	m_simulator->addSimulationProcess(std::move(simProc));
}

void UnitTestSimulationFixture::addSimulationFiber(std::function<void()> simFiber)
{
	m_simulator->addSimulationFiber(std::move(simFiber));
}

void UnitTestSimulationFixture::eval(hlim::Circuit &circuit)
{
	m_simulator->compileProgram(circuit);
	m_simulator->powerOn();
	//m_simulator->reevaluate();
	m_simulator->commitState();

	if (!m_errors.empty())
		BOOST_FAIL(m_errors.front());
	if (!m_warnings.empty())
		BOOST_ERROR(m_warnings.front());
}

void UnitTestSimulationFixture::runTicks(hlim::Circuit &circuit, const hlim::Clock *clock, unsigned numTicks)
{
	m_runLimClock = 0;
	m_runLimClock = clock;

	m_simulator->compileProgram(circuit);
	m_simulator->powerOn();
	m_simulator->advance(hlim::ClockRational(numTicks) / clock->absoluteFrequency());
	m_simulator->commitState();

	if (!m_errors.empty())
		BOOST_FAIL(m_errors.front());
	if (!m_warnings.empty())
		BOOST_ERROR(m_warnings.front());
}

void UnitTestSimulationFixture::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
	BOOST_TEST_MESSAGE(msg);
}

void UnitTestSimulationFixture::onWarning(const hlim::BaseNode *src, std::string msg)
{
	m_warnings.push_back(msg);
}

void UnitTestSimulationFixture::onAssert(const hlim::BaseNode *src, std::string msg)
{
	m_errors.push_back(msg);
}

}
