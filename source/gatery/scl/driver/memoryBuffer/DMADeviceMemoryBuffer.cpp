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

#include "DMADeviceMemoryBuffer.h"


/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

DMADeviceMemoryBuffer::DMADeviceMemoryBuffer(DMAMemoryBufferFactory &factory) : m_factory(factory)
{
}
			
std::span<std::byte> DMADeviceMemoryBuffer::lock(Flags flags)
{
	checkFlags(flags);

	if (m_uploadBuffer != nullptr) throw std::runtime_error("Buffer is already locked!");
	m_uploadBuffer = m_factory.allocateUploadBuffer(m_size);

	if (m_uploadBuffer->size() != m_size) throw std::runtime_error("Upload buffer has wrong size!");
	if (m_uploadBuffer->pageSize() % m_accessAlignment != 0) throw std::runtime_error("Page size does not align with access alignment of device buffer!");

	auto res = m_uploadBuffer->lock(flags);

	m_lockFlags = flags;

	if ((std::uint32_t)m_lockFlags & (std::uint32_t)Flags::DISCARD == 0) {
		size_t pageSize = m_uploadBuffer->pageSize();
		size_t numFullPages = (size_t)(m_size / pageSize);
		for (size_t page = 0; page < numFullPages; page++)
			downloadContinuousChunk(m_uploadBuffer->physicalPageStart(page), pageSize * page, pageSize);

		size_t remaining = (size_t)(m_size % pageSize);
		if (remaining > 0)
			downloadContinuousChunk(m_uploadBuffer->physicalPageStart(numFullPages), pageSize * numFullPages, remaining);
	}

	return res;
}

void DMADeviceMemoryBuffer::unlock()
{
	if (m_uploadBuffer == nullptr) throw std::runtime_error("Buffer is not locked!");
	m_uploadBuffer->unlock();

	if ((std::uint32_t)m_lockFlags & (std::uint32_t)Flags::READ_ONLY == 0) {
		size_t pageSize = m_uploadBuffer->pageSize();
		size_t numFullPages = (size_t)(m_size / pageSize);
		for (size_t page = 0; page < numFullPages; page++)
			uploadContinuousChunk(m_uploadBuffer->physicalPageStart(page), pageSize * page, pageSize);

		size_t remaining = (size_t)(m_size % pageSize);
		if (remaining > 0)
			uploadContinuousChunk(m_uploadBuffer->physicalPageStart(numFullPages), pageSize * numFullPages, remaining);
	}
}

void DMADeviceMemoryBuffer::write(std::span<const std::byte> data)
{
	if (!m_canWrite)
		throw std::runtime_error("Buffer can not be written!");

	if (data.size() > m_size)
		throw std::runtime_error("Too much data for memory buffer!");

	if (data.size() % m_accessAlignment != 0)
		throw std::runtime_error("Data smount does not match access alignment constraints!");

	auto frontPageBuffer = m_factory.allocateUploadBuffer(m_factory.pageSize());
	auto backPageBuffer = m_factory.allocateUploadBuffer(m_factory.pageSize());

	auto frontPageBufferAddr = frontPageBuffer->physicalPageStart(0);
	auto backPageBufferAddr = backPageBuffer->physicalPageStart(0);


	std::uint64_t pageSize = m_uploadBuffer->pageSize();
	std::uint64_t numPages = (data.size() + pageSize-1) / pageSize;
	for (std::uint64_t page = 0; page < numPages; page++) {
		std::uint64_t chunkSize = std::min(pageSize, data.size() - (std::uint64_t) page * pageSize);
		frontPageBuffer->write(data.subspan(page * pageSize, chunkSize));
		uploadContinuousChunk(frontPageBufferAddr, pageSize * page, chunkSize);
		std::swap(frontPageBuffer, frontPageBuffer);
		std::swap(frontPageBufferAddr, backPageBufferAddr);
	}
}

void DMADeviceMemoryBuffer::read(std::span<std::byte> data) const
{
	if (!m_canRead)
		throw std::runtime_error("Buffer can not be read!");

	if (data.size() > m_size)
		throw std::runtime_error("Too much data for memory buffer!");

	if (data.size() % m_accessAlignment != 0)
		throw std::runtime_error("Data smount does not match access alignment constraints!");

	auto frontPageBuffer = m_factory.allocateUploadBuffer(m_factory.pageSize());
	auto backPageBuffer = m_factory.allocateUploadBuffer(m_factory.pageSize());

	auto frontPageBufferAddr = frontPageBuffer->physicalPageStart(0);
	auto backPageBufferAddr = backPageBuffer->physicalPageStart(0);

	std::uint64_t pageSize = m_uploadBuffer->pageSize();
	std::uint64_t numPages = (data.size() + pageSize-1) / pageSize;
	for (std::uint64_t page = 0; page < numPages; page++) {
		std::uint64_t chunkSize = std::min(pageSize, data.size() - (std::uint64_t) page * pageSize);
		downloadContinuousChunk(frontPageBufferAddr, pageSize * page, chunkSize);
		frontPageBuffer->read(data.subspan(page * pageSize, chunkSize));
		std::swap(frontPageBuffer, frontPageBuffer);
		std::swap(frontPageBufferAddr, backPageBufferAddr);
	}
}


std::unique_ptr<PinnedHostMemoryBuffer> DMAMemoryBufferFactory::allocateUploadBuffer(std::uint64_t bytes)
{
	return m_uploadBufferFactory.allocateDerived<PinnedHostMemoryBuffer>(bytes);
}

DMAMemoryBufferFactory::DMAMemoryBufferFactory(PinnedHostMemoryBufferFactory &uploadBufferFactory) : m_uploadBufferFactory(uploadBufferFactory)
{
}


}

/**@}*/