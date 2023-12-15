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
#pragma once

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include "LinuxAddressTranslator.h"

#include <cstddef>
#include <span>
#include <vector>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

	class LinuxDMABuffer {
		public:
			LinuxDMABuffer(LinuxAddressTranslator &addrTranslator, size_t size, bool continuous = false, size_t retries = 100);
			~LinuxDMABuffer();

			LinuxDMABuffer(const LinuxDMABuffer&) = delete;
			void operator=(const LinuxDMABuffer&) = delete;

			bool isContinuous() const;

			std::vector<PhysicalAddr> getScatterGatherList() const;

			std::span<std::byte> userSpaceBuffer() { return m_buffer; }
			
			inline PhysicalAddr userToPhysical(void *usrSpaceAddr) const { return m_addrTranslator.userToPhysical(usrSpaceAddr); }
		protected:
			std::span<std::byte> m_buffer;
			LinuxAddressTranslator &m_addrTranslator;

			void allocatePopulateLock(size_t size);
	};

}

/**@}*/