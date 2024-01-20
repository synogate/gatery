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

#include "TrickleDeviceMemoryBuffer.h"
#include <stdexcept>

namespace gtry::scl::driver {

TrickleDeviceMemoryBuffer::TrickleDeviceMemoryBuffer(std::uint64_t size, PhysicalAddr deviceAddr, DeviceMemoryAllocator &allocator) : DeviceMemoryBuffer(size, deviceAddr, allocator)
{
}

std::span<std::byte> TrickleDeviceMemoryBuffer::lock(Flags flags)
{
	checkFlags(flags);

	if (!m_uploadBuffer.empty()) throw std::runtime_error("Buffer is already locked!");
	m_uploadBuffer.resize(m_size);

	m_lockFlags = flags;

	if (((std::uint32_t)m_lockFlags & (std::uint32_t)Flags::DISCARD) == 0)
		read(m_uploadBuffer);

	return m_uploadBuffer;
}

void TrickleDeviceMemoryBuffer::unlock()
{
	if (m_uploadBuffer.empty()) throw std::runtime_error("Buffer is not locked!");

	if (((std::uint32_t)m_lockFlags & (std::uint32_t)Flags::READ_ONLY) == 0)
		write(m_uploadBuffer);
}

TrickleDeviceMemoryBufferFactory::TrickleDeviceMemoryBufferFactory(DeviceMemoryAllocator &allocator) : DeviceMemoryBufferFactory(allocator) 
{
}

}

/**@}*/