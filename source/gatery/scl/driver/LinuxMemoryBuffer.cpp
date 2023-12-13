+/*  This file is part of Gatery, a library for circuit design.
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

#include "LinuxMemoryBuffer.h"

#include <cstring>
#include <sys/mman.h>

namespace gtry::scl::driver {

namespace {

void unlockAndUnmap(std::span<std::byte> buffer)
{
	if (!buffer.empty()) {
		munlock((void*) buffer.data(), buffer.size());
		munmap((void*) buffer.data(), buffer.size());
	}
}

struct MemoryToUnlockAndUnmap {
	std::vector<std::span<std::byte>> chunks;
	~MemoryToUnlockAndUnmap() {
		for (auto &c : chunks)
			unlockAndUnmap(c);
	}
};

}


LinuxMemoryBuffer::LinuxMemoryBuffer(LinuxAddressTranslator &addrTranslator, size_t size, bool continuous, size_t retries) : m_addrTranslator(addrTranslator)
{
	allocatePopulateLock(size);

	if (continuous) {
		MemoryToUnlockAndUnmap attempts;

		for (size_t i = 0; i < retries; i++) {
			if (isContinuous())	
				return;
			
			attempts.chunks.push_back(m_buffer);
			m_buffer = {};
			allocatePopulateLock(size);
		}
		throw std::runtime_error("Failed to allocate continuous memory!");
	}
}

LinuxMemoryBuffer::~LinuxMemoryBuffer()
{
	unlockAndUnmap(m_buffer);
}

bool LinuxMemoryBuffer::isContinuous() const
{
	PhysicalAddress startAddress = userToPhysical(m_buffer.data());
	size_t numPages = (m_buffer.size() + m_addrTranslator.pageSize()-1) / m_addrTranslator.pageSize();
	
	for (size_t page = 1; page < numPages; page++) {
		uint64_t offset = page * m_addrTranslator.pageSize();
		if (userToPhysical(m_buffer.data() + offset) != startAddress + offset)
			return false;
	}
	return true;
}

void LinuxMemoryBuffer::allocatePopulateLock(size_t size)
{
	void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (addr == nullptr)
		throw std::runtime_error("Failed to allocate!");
	else {
		m_buffer = std::span<std::byte>(addr, size);

		// Force pages into existance by writing to them.
		std::memset(m_buffer.data(), 0, m_buffer.size());

		if (mlock(m_buffer.data(), m_buffer.size()) != 0)
			throw std::runtime_error("Pinning memory failed!");
	}
}

}
