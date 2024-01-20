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

#include <gatery/frontend/ExternalModule.h>
#include <gatery/scl/axi/AxiMemorySimulation.h>

#include "../../axi/axi.h"

namespace gtry::scl::arch::xilinx
{
	class HBM_IP : public ExternalModule
	{
	public:
		HBM_IP(std::string_view ipName = "hbm_0");

		// this clocks needs to be connected even when not using the APB interface
		void clockAPB(const Clock& clk, size_t stackIndex);
		void clockRef(const Clock& clk, size_t stackIndex);

		Axi4 port(size_t portIndex, BitWidth addrW = 33_b, bool addECCBitsToData = false);

		Bit catastrophicTemperature(size_t stackIndex);
		UInt temperature(size_t stackIndex);
		Bit abpComplete(size_t stackIndex);
	protected:
		std::optional<Clock> m_controllerClock;
		Bit m_controllerResetLow;
		AxiMemorySimulationConfig m_memoryConfig;
	};
}
