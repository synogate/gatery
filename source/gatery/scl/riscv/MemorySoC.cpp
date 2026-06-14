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
#include "gatery/scl_pch.h"
#include "MemorySoC.h"

#include "DualCycleRV.h"
#include "../memoryMap/TileLinkMemoryMap.h"
#include "../tilelink/TileLinkCache.h"

#include "PipelineRV.h"

using namespace gtry;
using namespace gtry::scl;

MemorySoC& gtry::scl::MemorySoC::attachDevice(uint64_t baseAddress, TileLinkU&& device)
{
	if (baseAddress >= ioBaseAddress())
	{
		m_ioRouter(baseAddress - ioBaseAddress(), move(device));
	}
	else
	{
		m_cpuRouter(baseAddress, move(device));
	}
	return *this;
}

TileLinkU& gtry::scl::MemorySoC::createDmaPort(std::string portName, BitWidth sourceW, size_t burstLimitLog)
{
	m_dmaDevices.emplace(
		m_dmaDevices.begin(),
		tileLinkInit(
			{ .addrW = m_addrW, .dataW = 32_b, .sourceW = sourceW, .burstLimitLog = burstLimitLog }
		) | name(portName)
	);
	return m_dmaDevices.front();
}

void gtry::scl::MemorySoC::generate()
{
	attachDevice(ioBaseAddress(), *toTileLinkUL(m_mmap, 32_b, 9_b) | tileLinkRegDecouple);
	generateFirmwareMemory();
	m_cpuRouter(ioBaseAddress(), m_ioRouter.generate("ioRouter") | tileLinkRegDecouple);

	TileLinkArbiter cpuArbiter(m_cpuRouter.generate("cpuRouter"), 2_b);
	generateCpu(cpuArbiter);

	for (TileLinkU& dma : m_dmaDevices)
		cpuArbiter.out() <<= dma;
	cpuArbiter.generate();
}

void gtry::scl::MemorySoC::generateFirmwareMemory()
{
	formatCppHeader(m_firmwareBuildRoot / "gen_mmap_c.h", *m_mmap.getTree().physicalDescription, 32_b, ioBaseAddress());
	formatCppHeaderStructs(m_firmwareBuildRoot / "gen_mmap.h", *m_mmap.getTree().physicalDescription, 32_b, ioBaseAddress());
	riscv::ElfLoader elf = riscv::makeElf("firmware.elf", m_firmwareBuildRoot);

	riscv::ElfLoader::MegaSegment segment = elf.segments(0, 7, 0);
	const riscv::ElfLoader::Section* textSection = elf.section(".text");

	sim::DefaultBitVectorState segmentContent = segment.memoryState();
	// erase elf header
	size_t removeElfBits = (textSection->offset - segment.offset) * 8;
	segmentContent = segmentContent.extract(removeElfBits, segmentContent.size() - removeElfBits);

	{
		const uint64_t textStart = textSection->offset;
		const uint64_t textEnd = textSection->offset + segmentContent.size() / 8;
		const uint64_t ioStart = ioBaseAddress();
		const uint64_t ioEnd = ioStart + m_mmap.getTree().physicalDescription->size.bytes();

		// if you hit one of the following exceptions you should move the firmware text section to a different offset
		// GCC: -Wl,--section-start=.text=0x9000000
		// it is a good idea to chose an offset that is in the io range (address high bit set) for improved system timing
		HCL_DESIGNCHECK_HINT(((utils::nextPow2(segmentContent.size() / 8) - 1) & textStart) == 0, "text section offset needs to natural aligned");
		HCL_DESIGNCHECK_HINT(textStart >= ioEnd || textEnd <= ioStart, "text offset ovverlaps with mmap range");
		HCL_DESIGNCHECK_HINT((m_allowFirmwareInCache && textStart == 0) || textStart >= ioEnd, "text offset is not in io range. this check is just for improved Fmax and can be removed if needed.")
	}

	bool needWritePort = elf.segments().size() > 1;
	attachFirmwareMemory(elf.entryPoint(), textSection->offset, move(segmentContent), needWritePort);
}

void gtry::scl::MemorySoC::attachFirmwareMemory(uint64_t entryPoint, uint64_t offset, sim::DefaultBitVectorState content, bool writePort)
{
	m_entryPoint = entryPoint;

	if (m_allowFirmwareInCache && offset == 0 && !writePort)
	{
		m_firmwareRom = move(content);
	}
	else
	{
		if (m_allowFirmwareInCache)
			std::cerr << "warning: MemorySoC firmware in cache enabled but not supported by elf\n";

		Memory<BVec> textMem = memoryFromData(content, 32_b);
		attachDevice(offset, tileLinkMemory(textMem, { .sourceW = 9_b, .writePort = writePort, .latency = 2 }));
	}
}

void gtry::scl::MemorySoC::generateCpu(TileLinkArbiter& arbiter)
{
	TileLinkU iFetchPort = arbiter.out() | name("iFetchPort");
	size_t maxBurst = std::min<size_t>(4, iFetchPort.cfg.burstLimitLog);

	std::function<void(TileLinkChannelA&)> burstSelect = [&](TileLinkChannelA& a) {
		// disable bursts on io address space
		a->size = 2;
		IF(!a->address.msb())
			for (size_t i = 3; i <= maxBurst; ++i)
				IF(a->address.lower(BitWidth{ i }) == 0)
					a->size = i;
	};

	BitWidth addrW = std::max(9_b, BitWidth::count(m_firmwareRom.size() / 32));
	TileLinkU iCachePort = tileLinkCacheDirectMappedReadOnlyBurst(move(iFetchPort), addrW, burstSelect, m_firmwareRom) | name("iCachePort");
	
#if 0
	riscv::DualCycleRV userCpu(m_addrW, m_addrW);
	userCpu.fetch(move(iCachePort), m_entryPoint);
	userCpu.execute();
	userCpu.mem(arbiter);
#else
	rivet::PipelineRV userCpu;
	userCpu.instrStartAddress(m_entryPoint);
	userCpu.environmentCallAddress(m_entryPoint);
	userCpu.instrFetch(move(iCachePort));
	userCpu.dataMem(arbiter);
	userCpu.generate();
#endif
}
