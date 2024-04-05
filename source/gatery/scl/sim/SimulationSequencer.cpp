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
#include "gatery/scl_pch.h"
#include "SimulationSequencer.h"
namespace gtry::scl {

	SimulationSequencerSlot SimulationSequencer::allocate(){
		return { m_data, m_data->slotTotal++ };
	}

	SimulationSequencerSlot::SimulationSequencerSlot(SimulationSequencerSlot&& other) :
		m_data(std::move(other.m_data)),
		m_mySlot(other.m_mySlot)
	{
		other.m_data.reset();
	}

	SimulationSequencerSlot::~SimulationSequencerSlot() noexcept(false)
	{
		if(m_data && !simulationIsShuttingDown())
		{
			HCL_ASSERT(m_data->slotCurrent == m_mySlot);
			m_data->slotCurrent++;
			m_data->waitCondition.notify_all();
		}
	}

	SimFunction<void> SimulationSequencerSlot::wait()
	{
		while (m_data->slotCurrent != m_mySlot) {
			co_await m_data->waitCondition.wait();
		}
	}

}
