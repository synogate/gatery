/*  This file is part of Gatery, a library for circuit design.
Copyright (C) 2024 Michael Offel, Andreas Ley

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

#include <gatery/scl/driver/memoryBuffer/PinnedHostMemoryBuffer.h>
#include <gatery/scl/driver/memoryBuffer/DMADeviceMemoryBuffer.h>
#include <gatery/hlim/postprocessing/MemoryStorage.h>

namespace gtry::scl::sim::driver {

	class SimuPinnedHostMemoryBuffer : public gtry::scl::driver::PinnedHostMemoryBuffer {
		public:
			SimuPinnedHostMemoryBuffer(gtry::scl::driver::DeviceMemoryAllocator &allocator, hlim::MemoryStorage &hostMemoryStorage, gtry::scl::driver::PhysicalAddr physicalAddr, std::uint64_t bytes);
			virtual ~SimuPinnedHostMemoryBuffer();

			virtual gtry::scl::driver::PhysicalAddr physicalPageStart(size_t page) const override;

			virtual std::span<std::byte> lock(Flags flags) override;
			virtual void unlock() override;

			virtual void write(std::span<const std::byte> data) override;
			virtual void read(std::span<std::byte> data) const override;
		protected:
			gtry::scl::driver::DeviceMemoryAllocator &m_allocator;
			hlim::MemoryStorage &m_hostMemoryStorage;
			gtry::scl::driver::PhysicalAddr m_physicalAddr;
			std::vector<std::byte> m_shadowBuffer;
			Flags m_lockFlags;
	};

	class SimuPinnedHostMemoryBufferFactory : public gtry::scl::driver::PinnedHostMemoryBufferFactory {
		public:
			SimuPinnedHostMemoryBufferFactory(hlim::MemoryStorage &hostMemoryStorage, std::uint64_t pinnedMemoryStart);

			virtual std::unique_ptr<gtry::scl::driver::MemoryBuffer> allocate(uint64_t bytes) override;
		protected:
			hlim::MemoryStorage &m_hostMemoryStorage;
			std::unique_ptr<gtry::scl::driver::DeviceMemoryAllocator> m_allocator;
	};	
}
