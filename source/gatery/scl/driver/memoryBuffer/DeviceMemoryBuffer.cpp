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

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include "DeviceMemoryBuffer.h"

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

DeviceMemoryBuffer::DeviceMemoryBuffer(std::uint64_t size, PhysicalAddr deviceAddr, DeviceMemoryAllocator &allocator) : MemoryBuffer(size), m_deviceAddr(deviceAddr), m_allocator(allocator)
{
}

DeviceMemoryBuffer::~DeviceMemoryBuffer()
{
	m_allocator.free(m_deviceAddr, m_size);
}


DeviceMemoryBufferFactory::DeviceMemoryBufferFactory(DeviceMemoryAllocator &allocator) : m_allocator(allocator)
{
}

std::unique_ptr<MemoryBuffer> DeviceMemoryBufferFactory::alias(PhysicalAddr deviceAddr, uint64_t bytes)
{
	m_allocator.reserve(deviceAddr, bytes);
	return createBuffer(deviceAddr, bytes);
}

std::unique_ptr<MemoryBuffer> DeviceMemoryBufferFactory::allocate(uint64_t bytes)
{
	return createBuffer(m_allocator.allocate(bytes), bytes);
}



}

/**@}*/