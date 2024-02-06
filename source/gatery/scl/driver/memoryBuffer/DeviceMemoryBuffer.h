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


/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

	class DeviceMemoryAllocator {
		public:
			virtual PhysicalAddr allocate(uint64_t bytes, uint64_t alignment = 1) = 0;
			virtual void reserve(PhysicalAddr deviceAddr, uint64_t bytes) = 0;
			virtual void free(PhysicalAddr deviceAddr, uint64_t bytes) = 0;
	};


	class DummyDeviceMemoryAllocator : public DeviceMemoryAllocator {
		public:
			DummyDeviceMemoryAllocator(std::uint64_t nextAlloc = 0) : m_nextAlloc(nextAlloc) { }

			virtual PhysicalAddr allocate(uint64_t bytes, uint64_t alignment) override {
				auto res = (m_nextAlloc + alignment-1)/alignment*alignment;
				m_nextAlloc = res + bytes;
				return res;
			}
			virtual void reserve(PhysicalAddr deviceAddr, uint64_t bytes) override {  } //m_nextAlloc = std::max(m_nextAlloc, deviceAddr + bytes); }
			virtual void free(PhysicalAddr deviceAddr, uint64_t bytes) override { }
		protected:
			std::uint64_t m_nextAlloc = 0;
	};

	class DeviceMemoryBuffer : public MemoryBuffer {
		public:
			DeviceMemoryBuffer(std::uint64_t size, PhysicalAddr deviceAddr, DeviceMemoryAllocator &allocator);
			virtual ~DeviceMemoryBuffer();

			inline PhysicalAddr deviceAddr() const { return m_deviceAddr; }
		protected:
			PhysicalAddr m_deviceAddr = 0;
			DeviceMemoryAllocator &m_allocator;
	};

	class DeviceMemoryBufferFactory : public MemoryBufferFactory {
		public:
			DeviceMemoryBufferFactory(DeviceMemoryAllocator &allocator);
			virtual ~DeviceMemoryBufferFactory() = default;

			virtual std::unique_ptr<MemoryBuffer> alias(PhysicalAddr deviceAddr, uint64_t bytes);
			virtual std::unique_ptr<MemoryBuffer> allocate(uint64_t bytes) override;

			inline auto allocateDerived(uint64_t bytes) { return allocateDerivedImpl<DeviceMemoryBuffer>(bytes); }
			inline auto aliasDerived(PhysicalAddr deviceAddr, uint64_t bytes) {
				auto buf = alias(deviceAddr, bytes);
				return std::unique_ptr<DeviceMemoryBuffer>(static_cast<DeviceMemoryBuffer*>(buf.release()));
			}
		protected:
			DeviceMemoryAllocator &m_allocator;
			virtual std::unique_ptr<MemoryBuffer> createBuffer(PhysicalAddr deviceAddr, uint64_t bytes) = 0;
	};

}

/**@}*/
