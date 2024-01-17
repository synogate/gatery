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

#include "MemoryBuffer.h"
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

	class DMADeviceMemoryBuffer : public MemoryBuffer {
		public:
			DMADeviceMemoryBuffer(DMAMemoryBufferFactory &factory);
			
			virtual std::span<std::byte> lock(Flags flags) override;
			virtual void unlock() override;
			virtual void write(std::span<const std::byte> data) override;
			virtual void read(std::span<std::byte> data) const override;
		protected:
			DMAMemoryBufferFactory &m_factory;
			std::unique_ptr<PinnedHostMemoryBuffer> m_uploadBuffer;
			Flags m_lockFlags;

			virtual void uploadContinuousChunk(PhysicalAddr hostPageStart, PhysicalAddr deviceBufferOffset, size_t size) = 0;
			virtual void downloadContinuousChunk(PhysicalAddr hostPageStart, PhysicalAddr deviceBufferOffset, size_t size) const = 0;
	};

	class DMAMemoryBufferFactory : public MemoryBufferFactory {
		public:
			DMAMemoryBufferFactory(PinnedHostMemoryBufferFactory &uploadBufferFactory);

			std::unique_ptr<PinnedHostMemoryBuffer> allocateUploadBuffer(std::uint64_t bytes);

			inline size_t pageSize() const { return m_uploadBufferFactory.pageSize(); }
		protected:
			PinnedHostMemoryBufferFactory &m_uploadBufferFactory;
	};


}

/**@}*/