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

#include "AddressTranslator.h"

#include "../memoryBuffer/MemoryBuffer.h"

#include <cstddef>
#include <span>
#include <vector>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {
	typedef std::uint64_t PhysicalAddr;
}

namespace gtry::scl::driver::lnx {

	class PinnedMemory {
		public:
			PinnedMemory(AddressTranslator &addrTranslator, size_t size, bool continuous = false, size_t retries = 100);
			~PinnedMemory();

			PinnedMemory(const PinnedMemory&) = delete;
			PinnedMemory(PinnedMemory&& other);
			void operator=(const PinnedMemory&) = delete;

			bool isContinuous() const;
			void writeBackDCache() const;

			inline size_t size() const { return m_buffer.size(); }
			inline size_t pageSize() const { return m_addrTranslator.pageSize(); }

			std::span<std::byte> userSpaceBuffer() { return m_buffer; }
			std::span<const std::byte> userSpaceBuffer() const { return m_buffer; }
			inline PhysicalAddr userToPhysical(void *usrSpaceAddr) const { return m_addrTranslator.userToPhysical(usrSpaceAddr); }

			std::vector<PhysicalAddr> getScatterGatherList() const;
		protected:
			std::span<std::byte> m_buffer;
			AddressTranslator &m_addrTranslator;

			void allocatePopulateLock(size_t size);
	};

}

/**@}*/