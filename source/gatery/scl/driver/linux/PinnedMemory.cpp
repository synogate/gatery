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

#include "PinnedMemory.h"

#include <cstring>
#include <sys/mman.h>

#include <stdexcept>

#if defined(__x86_64__) || defined(__i386__)
#include <x86gprintrin.h>
#endif

namespace gtry::scl::driver::lnx {

namespace {

void unlockAndUnmap(std::span<std::byte> buffer)
{
	if (!buffer.empty()) {
		munlock((void*) buffer.data(), buffer.size());
		munmap((void*) buffer.data(), buffer.size());
	}
}

struct MemoryToUnlockAndUnmap {
	std::vector<std::span<std::byte>> freeInDtor;
	~MemoryToUnlockAndUnmap() {
		for (auto &c : freeInDtor)
			unlockAndUnmap(c);
	}
};

}


PinnedMemory::PinnedMemory(AddressTranslator &addrTranslator, size_t size, bool continuous, size_t retries) : m_addrTranslator(addrTranslator)
{
	allocatePopulateLock(size);

	// This is probably a stupid idea...
	if (continuous) {
		MemoryToUnlockAndUnmap attempts;
		for (size_t i = 0; i < retries; i++) {
			if (isContinuous())	
				return;
			
			attempts.freeInDtor.push_back(m_buffer);
			m_buffer = {};
			allocatePopulateLock(size);
		}
		throw std::runtime_error("Failed to allocate continuous memory!");
	}
}

PinnedMemory::PinnedMemory(PinnedMemory&& other) : m_addrTranslator(other.m_addrTranslator)
{
	std::swap(m_buffer, other.m_buffer);
}

PinnedMemory::~PinnedMemory()
{
	unlockAndUnmap(m_buffer);
}

bool PinnedMemory::isContinuous() const
{
	PhysicalAddr startAddress = userToPhysical(m_buffer.data());
	size_t numPages = (m_buffer.size() + m_addrTranslator.pageSize()-1) / m_addrTranslator.pageSize();
	
	for (size_t page = 1; page < numPages; page++) {
		uint64_t offset = page * m_addrTranslator.pageSize();
		if (userToPhysical(m_buffer.data() + offset) != startAddress + offset)
			return false;
	}
	return true;
}

std::vector<PhysicalAddr> PinnedMemory::getScatterGatherList() const
{
	size_t numPages = (m_buffer.size() + m_addrTranslator.pageSize()-1) / m_addrTranslator.pageSize();

	std::vector<PhysicalAddr> list;
	list.resize(numPages);
	for (size_t page = 0; page < numPages; page++)
		list[page] = userToPhysical(m_buffer.data() + page * m_addrTranslator.pageSize());

	return list;
}

void PinnedMemory::allocatePopulateLock(size_t size)
{
	//	MAP_HUGE_2MB, MAP_HUGE_1GB
	// Checking for MAP_SYNC availability, since it might not available on all systems.
	std::byte *addr = (std::byte *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS
	#ifdef MAP_SYNC
	| MAP_SYNC
	#endif
	, 0, 0);
	if (addr == nullptr)
		throw std::runtime_error("Failed to allocate!");
	else {
		m_buffer = std::span(addr, size);

		// Force pages into existance by writing to them.
		//std::memset(m_buffer.data(), 0, m_buffer.size());
		std::ranges::fill(m_buffer, std::byte{0});

		if (mlock(m_buffer.data(), m_buffer.size()) != 0)
			throw std::runtime_error("Pinning memory failed!");
	}
}
/*
void PinnedMemory::writeBackDCache() const
{
#if defined(__x86_64__) || defined(__i386__)
	_mm_sfence();
	size_t cacheLineSize = 64;
	for (std::uint8_t *lineStart = m_buffer.data(); lineStart+cacheLineSize <= m_buffer.data() + m_buffer.size(); lineStart += cacheLineSize)
		_mm_clwb(lineStart);
#endif
}
*/

}
