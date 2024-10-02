/*  This file is part of Gatery, a library for circuit design.
Copyright (C) 2021 Michael Offel, Andreas Ley

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
#include <gatery/scl_pch.h>

#include "SimuPinnedHostMemoryBuffer.h"
#include "SimuPinnedHostMemoryBuffer.h"


namespace gtry::scl::sim::driver {


SimuPinnedHostMemoryBuffer::SimuPinnedHostMemoryBuffer(gtry::scl::driver::DeviceMemoryAllocator &allocator, hlim::MemoryStorage &hostMemoryStorage, gtry::scl::driver::PhysicalAddr physicalAddr, std::uint64_t bytes)
			: PinnedHostMemoryBuffer({}, 4096), m_allocator(allocator), m_hostMemoryStorage(hostMemoryStorage), m_physicalAddr(physicalAddr)
{
	m_size = bytes;
	m_shadowBuffer.resize(bytes);
	m_buffer = m_shadowBuffer;
}

SimuPinnedHostMemoryBuffer::~SimuPinnedHostMemoryBuffer()
{
	m_allocator.free(m_physicalAddr, m_size);
}

gtry::scl::driver::PhysicalAddr SimuPinnedHostMemoryBuffer::physicalPageStart(size_t page) const
{
	return m_physicalAddr + page * m_pageSize;
}

std::span<std::byte> SimuPinnedHostMemoryBuffer::lock(Flags flags)
{
	if (((std::uint32_t)flags & (std::uint32_t)Flags::DISCARD) == 0)
		read(m_buffer);

	m_lockFlags = flags;
	return PinnedHostMemoryBuffer::lock(flags);
}

void SimuPinnedHostMemoryBuffer::unlock()
{
	PinnedHostMemoryBuffer::unlock();

	if (((std::uint32_t)m_lockFlags & (std::uint32_t)Flags::READ_ONLY) == 0)
		write(m_buffer);
}

void SimuPinnedHostMemoryBuffer::write(std::span<const std::byte> data)
{
	if (data.size() > m_size) throw std::runtime_error("Wrong amount of data!");

	m_hostMemoryStorage.write(m_physicalAddr*8, gtry::sim::createDefaultBitVectorState(data.size()*8, data.data()), false, {});
}

void SimuPinnedHostMemoryBuffer::read(std::span<std::byte> data) const
{
	if (data.size() > m_size) throw std::runtime_error("Wrong amount of data!");

	auto chunk = m_hostMemoryStorage.read(m_physicalAddr*8, data.size());
	std::string_view undefined = "Undefined Value";

	gtry::sim::asData(chunk, data, std::span<const std::byte>((const std::byte*)undefined.data(), undefined.size()));
}

	

SimuPinnedHostMemoryBufferFactory::SimuPinnedHostMemoryBufferFactory(hlim::MemoryStorage &hostMemoryStorage, std::uint64_t pinnedMemoryStart)
	: m_hostMemoryStorage(hostMemoryStorage)
{
	m_allocator = std::make_unique<gtry::scl::driver::DummyDeviceMemoryAllocator>(pinnedMemoryStart);
	m_pageSize = 4096;
}

std::unique_ptr<gtry::scl::driver::MemoryBuffer> SimuPinnedHostMemoryBufferFactory::allocate(uint64_t bytes)
{
	return std::make_unique<SimuPinnedHostMemoryBuffer>(*m_allocator, m_hostMemoryStorage, m_allocator->allocate(bytes, m_pageSize), bytes);
}

}
