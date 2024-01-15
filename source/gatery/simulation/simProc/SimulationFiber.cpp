/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include "SimulationFiber.h"

namespace gtry::sim {

thread_local SimulationFiber *SimulationFiber::m_thisFiber;

SimulationFiber::SimulationFiber(SimulationCoroutineHandler &coroutineHandler, std::function<void()> body) : m_coroutineHandler(coroutineHandler), m_body(std::move(body))
{
}

SimulationFiber::~SimulationFiber()
{
	terminate();
	m_thread.join();
}

void SimulationFiber::start()
{

	std::unique_lock lock(m_mutex);
	m_terminate = false;
	m_threadRunning = true;
	m_thread = std::thread([this](){
		m_thisFiber = this;
		try {
			m_body();
		} catch (const SimulationTerminated &) {
		}
		std::unique_lock lock(m_mutex);
		m_threadRunning = false;
		m_wakeMain.notify_one();
	});

	while (m_threadRunning)
		m_wakeMain.wait(lock);
}

void SimulationFiber::suspend()
{
	std::unique_lock lock(m_mutex);
	m_threadRunning = false;
	m_wakeMain.notify_one();

	while (!m_threadRunning) {
		m_wakeFiber.wait(lock);
		if (m_terminate)
			throw SimulationTerminated{};
	}
}

void SimulationFiber::resume()
{
	std::unique_lock lock(m_mutex);
	m_threadRunning = true;
	m_wakeFiber.notify_one();

	while (m_threadRunning)
		m_wakeMain.wait(lock);
}

void SimulationFiber::terminate()
{
	std::unique_lock lock(m_mutex);
	m_terminate = true;
	m_wakeFiber.notify_one();

	while (m_threadRunning)
		m_wakeMain.wait(lock);
}

}
