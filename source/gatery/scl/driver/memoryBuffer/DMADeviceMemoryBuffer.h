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

#include "DeviceMemoryBuffer.h"
#include "PinnedHostMemoryBuffer.h"

#include <memory>

#include <cstddef>
#include <span>
#include <vector>
#include <cstdint>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

	class DMAMemoryBufferFactory;

	class DeviceDMAController {
		public:
			virtual void uploadContinuousChunk(PhysicalAddr hostAddr, PhysicalAddr deviceAddr, size_t size) = 0;
			virtual void downloadContinuousChunk(PhysicalAddr hostAddr, PhysicalAddr deviceAddr, size_t size) const = 0;
		protected:
			bool m_canUpload = true;
			bool m_canDownload = true;
			std::uint64_t m_accessAlignment = 1;
	};

	class DMADeviceMemoryBuffer : public DeviceMemoryBuffer {
		public:
			DMADeviceMemoryBuffer(DMAMemoryBufferFactory &factory, std::uint64_t bytes, PhysicalAddr deviceAddr, DeviceMemoryAllocator &allocator);
			
			virtual std::span<std::byte> lock(Flags flags) override;
			virtual void unlock() override;
			virtual void write(std::span<const std::byte> data) override;
			virtual void read(std::span<std::byte> data) const override;
		protected:
			DMAMemoryBufferFactory &m_factory;
			std::unique_ptr<PinnedHostMemoryBuffer> m_uploadBuffer;
			Flags m_lockFlags;
	};

	class DMAMemoryBufferFactory : public DeviceMemoryBufferFactory {
		public:
			DMAMemoryBufferFactory(DeviceMemoryAllocator &allocator, PinnedHostMemoryBufferFactory &uploadBufferFactory, DeviceDMAController &dmaController);

			std::unique_ptr<PinnedHostMemoryBuffer> allocateUploadBuffer(std::uint64_t bytes);
			DeviceDMAController &dmaController() { return m_dmaController; }

			inline size_t pageSize() const { return m_uploadBufferFactory.pageSize(); }

			inline auto allocateDerived(uint64_t bytes) { return allocateDerivedImpl<DMADeviceMemoryBuffer>(bytes); }
		protected:
			PinnedHostMemoryBufferFactory &m_uploadBufferFactory;
			DeviceDMAController &m_dmaController;
			virtual std::unique_ptr<MemoryBuffer> createBuffer(PhysicalAddr deviceAddr, uint64_t bytes) override;
	};


}

/**@}*/