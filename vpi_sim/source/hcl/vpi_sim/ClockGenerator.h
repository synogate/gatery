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
#pragma once
#include <cstdint>

namespace vpi_host
{
	class ClockGenerator
	{
	public:
		ClockGenerator(uint64_t clockSimInterval, void* vpiNetHandle);
		ClockGenerator(ClockGenerator&&) = delete;
		ClockGenerator(const ClockGenerator&) = delete;
		~ClockGenerator();

		void onTimeInterval();

	private:
		void* m_VpiNet;
		void* m_VpiCallback;
		uint64_t m_Interval;
	};

}
