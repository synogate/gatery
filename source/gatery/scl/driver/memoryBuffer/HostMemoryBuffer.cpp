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

#include "HostMemoryBuffer.h"

#include <algorithm>
#include <stdexcept>

namespace gtry::scl::driver {

HostMemoryBuffer::HostMemoryBuffer(uint64_t bytes) : MemoryBuffer(bytes)
{
	m_buffer.resize(bytes);
}

std::span<std::byte> HostMemoryBuffer::lock(Flags flags)
{
	return m_buffer;
}

void HostMemoryBuffer::unlock()
{
}

void HostMemoryBuffer::write(std::span<const std::byte> data)
{
	if (data.size() > m_buffer.size()) throw std::runtime_error("Too much data for buffer size");
	std::copy(data.begin(), data.end(), m_buffer.begin());
}

void HostMemoryBuffer::read(std::span<std::byte> data) const
{
	if (data.size() < m_buffer.size()) throw std::runtime_error("Too little data for buffer size");
	std::copy(m_buffer.begin(), m_buffer.end(), data.begin());
}

std::unique_ptr<MemoryBuffer> HostMemoryBufferFactory::allocate(uint64_t bytes)
{
	return std::make_unique<HostMemoryBuffer>(bytes);
}

}

/**@}*/