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

#include "MemoryBuffer.h"
#include <stdexcept>

namespace gtry::scl::driver {

LockedSpan::LockedSpan(MemoryBuffer &buffer, std::span<std::byte> data)
		: m_buffer(buffer), m_data(data)
{
}

LockedSpan::~LockedSpan()
{
	m_buffer.unlock();
}

ConstLockedSpan::ConstLockedSpan(MemoryBuffer &buffer, std::span<const std::byte> data)
		: m_buffer(buffer), m_data(data)
{
}

ConstLockedSpan::~ConstLockedSpan()
{
	m_buffer.unlock();
}


MemoryBuffer::MemoryBuffer(std::uint64_t size) : m_size(size)
{
}

void MemoryBuffer::checkFlags(Flags flags)
{
	if ((((std::uint32_t)flags & (std::uint32_t)Flags::DISCARD) == 0) && !m_canRead)
		throw std::runtime_error("The buffer can not be read and must be locked as DISCARD!");

	if ((((std::uint32_t)flags &(std::uint32_t)Flags::READ_ONLY) == 0) && !m_canWrite)
		throw std::runtime_error("The buffer can not be written and must be locked as READ_ONLY!");
}

void MemoryBuffer::writeFromBuffer(const MemoryBuffer &other)
{
	auto data = other.map(Flags::READ_ONLY);
	write(data.view<std::byte>());
}

void MemoryBuffer::readToBuffer(MemoryBuffer &other) const
{
	auto data = other.map(Flags::DISCARD);
	read(data.view<std::byte>());
}


}