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
#include <gatery/scl_pch.h>

#include "SimMemoryMap.h"

#include <gatery/simulation/Simulator.h>
#include <gatery/simulation/simProc/SimulationFiber.h>

#include <gatery/scl/tilelink/TileLinkMasterModel.h>

namespace gtry::scl::driver {

SimulationFiberMapped32BitTileLink::SimulationFiberMapped32BitTileLink(scl::TileLinkMasterModel &linkModel, const Clock &clock) : m_linkModel(linkModel), m_clock(clock)
{
}


uint8_t SimulationFiberMapped32BitTileLink::readU8(size_t addr) const
{
	return readU32(addr);
	//throw std::runtime_error("Can not make 8-bit reads!");
}

void SimulationFiberMapped32BitTileLink::writeU8(size_t addr, uint8_t data)
{
	writeU32(addr, data);
	return;
	//throw std::runtime_error("Can not make 8-bit writes!");
}

uint16_t SimulationFiberMapped32BitTileLink::readU16(size_t addr) const
{
	return readU32(addr);
	//throw std::runtime_error("Can not make 16-bit reads!");
}

void SimulationFiberMapped32BitTileLink::writeU16(size_t addr, uint16_t data)
{
	writeU32(addr, data);
//	throw std::runtime_error("Can not make 16-bit writes!");
}

uint32_t SimulationFiberMapped32BitTileLink::readU32(size_t addr) const
{
	return sim::SimulationFiber::awaitCoroutine<uint32_t>([&]()->sim::SimulationFunction<uint32_t> {
		auto [val, def, err] = co_await m_linkModel.get(addr, 2, m_clock);
		if (err) throw std::runtime_error("Bus error!");
		if (!def) throw std::runtime_error("Undefined value!");
		co_return (uint32_t) val;
	});
}

void SimulationFiberMapped32BitTileLink::writeU32(size_t addr, uint32_t data)
{
	sim::SimulationFiber::awaitCoroutine<uint32_t>([&]()->sim::SimulationFunction<uint32_t> {
		fork(m_linkModel.put(addr, 2, data, m_clock));
		co_return 0;
	});
}

uint64_t SimulationFiberMapped32BitTileLink::readU64(size_t addr) const
{
	// Force 32 bit accesses
	return ((uint64_t) readU32(addr)) | ((uint64_t) readU32(addr + 4)) << 32;
}

void SimulationFiberMapped32BitTileLink::writeU64(size_t addr, uint64_t data)
{
	// Force 32 bit accesses
	writeU32(addr, data & 0xFFFFFFFF);
	writeU32(addr + 4, (data >> 32) & 0xFFFFFFFF);
}

void SimulationFiberMapped32BitTileLink::readBlock(void *dst, size_t addr, size_t size) const
{
	if (addr % 4 != 0)
		throw std::runtime_error("Block reads must be 4-byte aligned!");

	if (size % 4 != 0)
		throw std::runtime_error("Block reads must be multiples of 4 bytes in size");

	uint32_t *dstWords = (uint32_t *) dst;
	for (size_t i = 0; i < size/4; i++)
		dstWords[i] = readU32(addr + i*4);
}

void SimulationFiberMapped32BitTileLink::writeBlock(const void *src, size_t addr, size_t size)
{
	if (addr % 4 != 0)
		throw std::runtime_error("Block reads must be 4-byte aligned!");

	if (size % 4 != 0)
		throw std::runtime_error("Block reads must be multiples of 4 bytes in size");

	const uint32_t *srcWords = (const uint32_t *) src;
	for (size_t i = 0; i < size/4; i++)
		writeU32(addr + i*4, srcWords[i]);
}


SimulationMapped32BitTileLink::SimulationMapped32BitTileLink(scl::TileLinkMasterModel &linkModel, const Clock &clock, sim::Simulator &simulator) : m_linkModel(linkModel), m_clock(clock), m_simulator(simulator)
{
}


uint8_t SimulationMapped32BitTileLink::readU8(size_t addr) const
{
	return readU32(addr);
	//throw std::runtime_error("Can not make 8-bit reads!");
}

void SimulationMapped32BitTileLink::writeU8(size_t addr, uint8_t data)
{
	writeU32(addr, data);
	return;
	//throw std::runtime_error("Can not make 8-bit writes!");
}

uint16_t SimulationMapped32BitTileLink::readU16(size_t addr) const
{
	return readU32(addr);
	//throw std::runtime_error("Can not make 16-bit reads!");
}

void SimulationMapped32BitTileLink::writeU16(size_t addr, uint16_t data)
{
	writeU32(addr, data);
//	throw std::runtime_error("Can not make 16-bit writes!");
}

uint32_t SimulationMapped32BitTileLink::readU32(size_t addr) const
{
	return m_simulator.executeCoroutine<uint32_t>([&]()->sim::SimulationFunction<uint32_t> {
		auto [val, def, err] = co_await m_linkModel.get(addr, 2, m_clock);
		if (err) throw std::runtime_error("Bus error!");
		if (!def) throw std::runtime_error("Undefined value!");
		co_return (uint32_t) val;
	});
}

void SimulationMapped32BitTileLink::writeU32(size_t addr, uint32_t data)
{
	m_simulator.executeCoroutine<uint32_t>([&]()->sim::SimulationFunction<uint32_t> {
		co_await m_linkModel.put(addr, 2, data, m_clock);
		co_return 0;
	});
}

uint64_t SimulationMapped32BitTileLink::readU64(size_t addr) const
{
	// Force 32 bit accesses
	return ((uint64_t) readU32(addr)) | ((uint64_t) readU32(addr + 4)) << 32;
}

void SimulationMapped32BitTileLink::writeU64(size_t addr, uint64_t data)
{
	// Force 32 bit accesses
	writeU32(addr, data & 0xFFFFFFFF);
	writeU32(addr + 4, (data >> 32) & 0xFFFFFFFF);
}

void SimulationMapped32BitTileLink::readBlock(void *dst, size_t addr, size_t size) const
{
	if (addr % 4 != 0)
		throw std::runtime_error("Block reads must be 4-byte aligned!");

	if (size % 4 != 0)
		throw std::runtime_error("Block reads must be multiples of 4 bytes in size");

	uint32_t *dstWords = (uint32_t *) dst;
	for (size_t i = 0; i < size/4; i++)
		dstWords[i] = readU32(addr + i*4);
}

void SimulationMapped32BitTileLink::writeBlock(const void *src, size_t addr, size_t size)
{
	if (addr % 4 != 0)
		throw std::runtime_error("Block reads must be 4-byte aligned!");

	if (size % 4 != 0)
		throw std::runtime_error("Block reads must be multiples of 4 bytes in size");

	const uint32_t *srcWords = (const uint32_t *) src;
	for (size_t i = 0; i < size/4; i++)
		writeU32(addr + i*4, srcWords[i]);
}


}
