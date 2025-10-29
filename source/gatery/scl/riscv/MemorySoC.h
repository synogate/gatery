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
#pragma once
#include "BaseSoC.h"
#include <filesystem>
#include "../tilelink/TileLinkInterconnect.h"
#include "../memoryMap/PackedMemoryMap.h"

namespace gtry::scl
{
	/**
	 * @brief Simple helper class to build the most basic form of memory SoC. In contrast to the embedded SoC, the memory SoC
	 *			has one central tile link hub where all masters connect to including the cpu instruction and data fetch.
	 *			For improved timing there is a second tile link decoder with more delay for io devices.
	 *
	 *			I Fetch, D Fetch, other Masters <-> CPU Hub (TL Arbiter + Decoder) <-> IO Decoder <-> IO Devices
	 *			The memory is connected to the CPU Hub for improved latency.
	 *
	 *			Addresses with high bit set are located in the IO range.
	 */
	class MemorySoC : public BaseSoC
	{
	public:
		enum class Decoder { io, cpu };

	public:
		// default off. if possible store firmware in cache RAM for faster startup and less onchip memory.
		// Note: any data load from firmware text will fail when enabled.
		MemorySoC& allowFirmwareInCache(bool value) { m_allowFirmwareInCache = value; return *this; }
		MemorySoC& firmwareBuildRoot(std::filesystem::path root) { m_firmwareBuildRoot = root; return *this; }
		uint64_t ioBaseAddress() const { return 1ull << (m_addrW.bits() - 1); }

		MemoryMap& mmap() override { return m_mmap; }
		MemorySoC& attachDevice(uint64_t baseAddress, TileLinkU&& device) override;
		TileLinkU& createDmaPort(std::string portName, BitWidth sourceW = 0_b, size_t burstLimitLog = 2) override;

		virtual void generate();

	protected:
		virtual void generateFirmwareMemory();
		virtual void attachFirmwareMemory(uint64_t entryPoint, uint64_t offset, sim::DefaultBitVectorState content, bool writePort);

		virtual void generateCpu(TileLinkArbiter& arbiter);

	private:
		BitWidth m_addrW = 28_b;
		std::filesystem::path m_firmwareBuildRoot = "firmware";
		bool m_allowFirmwareInCache = false;

		uint64_t m_entryPoint = 0;
		sim::DefaultBitVectorState m_firmwareRom;

		PackedMemoryMap m_mmap = PackedMemoryMap("dev");
		TileLinkAddressDecoder m_ioRouter;
		TileLinkAddressDecoder m_cpuRouter;
		std::list<TileLinkU> m_dmaDevices;
	};
}
