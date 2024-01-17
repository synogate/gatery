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
 
#include "MemoryMappedPCI.h"

#include "PCIDeviceFunctionList.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <stdexcept>

namespace gtry::scl::driver::lnx {

UserSpaceMapped32BitEndpoint::UserSpaceMapped32BitEndpoint(const PCIDeviceFunction &function, size_t size)
{
	std::string test = function.resource(0).string();
	auto fileDescriptor = open(function.resource(0).string().c_str(), O_RDWR);
	if (fileDescriptor == -1)
		throw std::runtime_error("Could not open pci-e device resource");
	void *mapping = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor, 0);
	close(fileDescriptor);

	if (mapping == nullptr)
		throw std::runtime_error("Could not memory map pci-e device resource");

	m_mappedRegisters = std::span<uint32_t>{ (uint32_t*) mapping, size };
}

UserSpaceMapped32BitEndpoint::~UserSpaceMapped32BitEndpoint()
{
	if (!m_mappedRegisters.empty())
		munmap((void*) m_mappedRegisters.data(), m_mappedRegisters.size());
}

uint8_t UserSpaceMapped32BitEndpoint::readU8(size_t addr) const
{
	return readU32(addr);
	//throw std::runtime_error("Can not make 8-bit reads!");
}

void UserSpaceMapped32BitEndpoint::writeU8(size_t addr, uint8_t data)
{
	writeU32(addr, data);
	return;
	//throw std::runtime_error("Can not make 8-bit writes!");
}

uint16_t UserSpaceMapped32BitEndpoint::readU16(size_t addr) const
{
	return readU32(addr);
	//throw std::runtime_error("Can not make 16-bit reads!");
}

void UserSpaceMapped32BitEndpoint::writeU16(size_t addr, uint16_t data)
{
	writeU32(addr, data);
//	throw std::runtime_error("Can not make 16-bit writes!");
}

uint32_t UserSpaceMapped32BitEndpoint::readU32(size_t addr) const
{
	return m_mappedRegisters[addr/4];
}

void UserSpaceMapped32BitEndpoint::writeU32(size_t addr, uint32_t data)
{
	m_mappedRegisters[addr/4] = data;
}

uint64_t UserSpaceMapped32BitEndpoint::readU64(size_t addr) const
{
	// Force 32 bit accesses
	return ((uint64_t) m_mappedRegisters[addr/4]) | ((uint64_t) m_mappedRegisters[addr/4 + 1]) << 32;
}

void UserSpaceMapped32BitEndpoint::writeU64(size_t addr, uint64_t data)
{
	// Force 32 bit accesses
	m_mappedRegisters[addr/4] = data & 0xFFFFFFFF;
	m_mappedRegisters[addr/4 + 1] = (data >> 32) & 0xFFFFFFFF;
}

void UserSpaceMapped32BitEndpoint::readBlock(void *dst, size_t addr, size_t size) const
{
	if (addr % 4 != 0)
		throw std::runtime_error("Block reads must be 4-byte aligned!");

	if (size % 4 != 0)
		throw std::runtime_error("Block reads must be multiples of 4 bytes in size");

	uint32_t *dstWords = (uint32_t *) dst;
	for (size_t i = 0; i < size/4; i++)
		dstWords[i] = m_mappedRegisters[addr/4 + i];
}

void UserSpaceMapped32BitEndpoint::writeBlock(const void *src, size_t addr, size_t size)
{
	if (addr % 4 != 0)
		throw std::runtime_error("Block reads must be 4-byte aligned!");

	if (size % 4 != 0)
		throw std::runtime_error("Block reads must be multiples of 4 bytes in size");

	const uint32_t *srcWords = (const uint32_t *) src;
	for (size_t i = 0; i < size/4; i++)
		m_mappedRegisters[addr/4 + i] = srcWords[i];
}


}