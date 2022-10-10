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
#include "SimulationProcess.h"
#include "../Simulator.h"

namespace gtry::sim {

thread_local SimulationCoroutineHandler *SimulationCoroutineHandler::activeHandler = nullptr;

SimulationCoroutineHandler::~SimulationCoroutineHandler()
{	
	stopAll();
}

void SimulationCoroutineHandler::stopAll()
{
	m_simulationCoroutines.clear();
	while (!m_coroutinesReadyToResume.empty())
		m_coroutinesReadyToResume.pop();
}


void SimulationCoroutineHandler::run()
{
	auto lastHandler = activeHandler;
	activeHandler = this;
	try {
		while (!m_coroutinesReadyToResume.empty()) {
			m_coroutinesReadyToResume.front().resume();
			m_coroutinesReadyToResume.pop();
		}
		collectGarbage();
	} catch (...) {
		activeHandler = lastHandler;
		throw;
	}
	activeHandler = lastHandler;
}

void SimulationCoroutineHandler::collectGarbage()
{
	// We hold references to keep fire&forget coroutines alive.
	// Once they are done, we drop the reference and if it was the last one, the promise gets deleted.
	for (size_t i = 0; i < m_simulationCoroutines.size(); ) {
		if (m_simulationCoroutines[i].done()) {
			m_simulationCoroutines[i] = m_simulationCoroutines.back();
			m_simulationCoroutines.pop_back();
		} else
			i++;
	}
}



template class SimulationFunction<void>;
template class SimulationFunction<int>;
template class SimulationFunction<size_t>;
template class SimulationFunction<bool>;

}
