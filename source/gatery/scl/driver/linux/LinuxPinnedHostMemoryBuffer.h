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

#include "../memoryBuffer/PinnedHostMemoryBuffer.h"

#include "AddressTranslator.h"
#include "PinnedMemory.h"

#include <map>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver::lnx {

	class LinuxPinnedHostMemoryBufferFactory;

	class LinuxPinnedHostMemoryBuffer : public PinnedHostMemoryBuffer {
		public:
			LinuxPinnedHostMemoryBuffer(LinuxPinnedHostMemoryBufferFactory &factory, PinnedMemory &&pinnedMemory);
			virtual ~LinuxPinnedHostMemoryBuffer();

			virtual PhysicalAddr physicalPageStart(size_t page) const override;
		protected:
			LinuxPinnedHostMemoryBufferFactory &m_factory;
			PinnedMemory m_pinnedMemory;
	};

	class LinuxPinnedHostMemoryBufferFactory : public PinnedHostMemoryBufferFactory {
		public:
			virtual std::unique_ptr<MemoryBuffer> allocate(uint64_t bytes) override;
			virtual void returnPinnedMemory(PinnedMemory &&pinnedMemory);

			inline auto allocateDerived(uint64_t bytes) { return allocateDerivedImpl<LinuxPinnedHostMemoryBuffer>(bytes); }
		protected:
			std::map<std::uint64_t, std::vector<PinnedMemory>> m_pool;
			AddressTranslator m_addrTranslator;
	};

}

/**@}*/