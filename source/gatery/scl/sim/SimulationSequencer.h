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
#pragma once
#include <gatery/frontend.h>


namespace gtry::scl
{
	class SimulationSequencerSlot;

	class SimulationSequencer
	{
	public:
		struct Data
		{
			size_t slotCurrent = 0;
			size_t slotTotal = 0;
			Condition waitCondition;
		};

		SimulationSequencerSlot allocate();

	private:
		std::shared_ptr<Data> m_data = std::make_shared<Data>();
	};

	class SimulationSequencerSlot
	{
	public:
		SimulationSequencerSlot(std::shared_ptr<SimulationSequencer::Data> data, size_t slot) : m_data(data), m_mySlot(slot) {}
		SimulationSequencerSlot(const SimulationSequencerSlot&) = delete;
		SimulationSequencerSlot(SimulationSequencerSlot&&);

		~SimulationSequencerSlot() noexcept(false);

		SimFunction<void> wait();

	private:
		std::shared_ptr<SimulationSequencer::Data> m_data;
		const size_t m_mySlot;
	};
}
