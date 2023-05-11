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
	auto lastHandler = activeHandler;
	activeHandler = this;
	m_simulationCoroutines.clear();
	while (!m_coroutinesReadyToResume.empty())
		m_coroutinesReadyToResume.pop();
	activeHandler = lastHandler;
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
		for (auto &h : m_simulationCoroutines)
			HCL_ASSERT(!h.done());
	} catch (...) {
		activeHandler = lastHandler;
		throw;
	}
	activeHandler = lastHandler;
}


template class SimulationFunction<void>;
template class SimulationFunction<int>;
template class SimulationFunction<size_t>;
template class SimulationFunction<bool>;

}
