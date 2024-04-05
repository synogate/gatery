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
#include "EmbeddedSystemBuilder.h"
#include "DualCycleRV.h"
#include "RiscVAssembler.h"

#include <boost/format.hpp>

using namespace gtry::scl::riscv;

static gtry::sim::DefaultBitVectorState stateRotateRight(const gtry::sim::DefaultBitVectorState& in, gtry::BitWidth amount)
{
	gtry::sim::DefaultBitVectorState ret;
	ret.resize(in.size());
	ret.copyRange(0, in, in.size() - amount.bits(), amount.bits());
	ret.copyRange(amount.bits(), in, 0, in.size() - amount.bits());
	return ret;
}

gtry::scl::riscv::EmbeddedSystemBuilder::EmbeddedSystemBuilder() :
	m_area{"EmbeddedSystem"}
{
	auto ent = m_area.enter();

	m_dataBus.address = 32_b;
	m_dataBus.read = Bit{};
	m_dataBus.write = Bit{};
	m_dataBus.writeData = 32_b;
	m_dataBus.byteEnable = 4_b;
	m_dataBus.readDataValid = Bit{};
	m_dataBus.readData = 32_b;
}

void gtry::scl::riscv::EmbeddedSystemBuilder::addCpu(const ElfLoader& orgelf, BitWidth scratchMemSize, bool dedicatedInstructionMemory, bool debugTrace)
{
	auto ent = m_area.enter();

	ElfLoader elf = orgelf;
	if(dedicatedInstructionMemory)
		elf.splitTextAndRoData();
	addDataMemory(elf, scratchMemSize);

	ElfLoader::MegaSegment codeMeg = elf.segments(1, 0, 0);
	if(codeMeg.subSections.empty())
	{
		m_dataBus.address = "32b0";
		m_dataBus.read = '0';
		m_dataBus.write = '0';
		m_dataBus.writeData = "32b0";
		m_dataBus.byteEnable = "0000";
		return; // no code -> no cpu
	}

	uint32_t entryPoint = (uint32_t)elf.entryPoint();
	ElfLoader::Segment initCodeSeg;
	if (!m_initCode.empty())
	{
		initCodeSeg.offset = codeMeg.offset + codeMeg.size.bytes();

		uint64_t jumpOffset = initCodeSeg.offset + m_initCode.size() * 4;
		m_initCode.push_back(assembler::jal(0, int32_t(entryPoint - jumpOffset)));

#if 0
		std::cout << "RV32 generated initialization code:\n";
		assembler::printCode(std::cout, m_initCode, initCodeSeg.offset);
#endif
		initCodeSeg.alignment = codeMeg.subSections.front()->alignment;
		initCodeSeg.flags = 1;
		initCodeSeg.size = BitWidth{ m_initCode.size() * 4 * 8 };
		initCodeSeg.data = std::span((const uint8_t*)m_initCode.data(), m_initCode.size() * 4);

		codeMeg.subSections.push_back(&initCodeSeg);
		codeMeg.size = codeMeg.size + m_initCode.size() * 8;

		entryPoint = (uint32_t)initCodeSeg.offset;
	}

	const Segment codeSeg = loadSegment(codeMeg, 0_b);
	DualCycleRV rv(codeSeg.addrWidth);
	rv.ipOffset(entryPoint & ~codeSeg.addrWidth.mask()); // set virtual high bits of IP for debugging

	Memory<UInt>& imem = rv.fetch(entryPoint & codeSeg.addrWidth.mask());
	imem.fillPowerOnState(codeSeg.resetState);
	
	rv.execute();

	setName(*m_dataBus.readDataValid, "databus_readdatavalid");
	setName(*m_dataBus.readData, "databus_readdata");
	rv.mem(m_dataBus);
	m_dataBus.setName("databus_");
	m_dataBus.readData = 0xFFFFFFFFu;
	m_dataBus.readDataValid = '0';

	HCL_NAMED(m_anyDeviceSelected);
	IF(!m_anyDeviceSelected)
		*m_dataBus.readDataValid = *m_dataBus.read;

	m_anyDeviceSelected = '0';

	if(debugTrace)
	{
		rv.trace().writeVcd();
	}
}

gtry::Bit gtry::scl::riscv::EmbeddedSystemBuilder::addUART(uint64_t offset, UART& config, const Bit& rx)
{
	auto ent = m_area.enter();

	UART::Stream txStream, rxStream = config.receive(rx);

	AvalonMM bus = addAvalonMemMapped(offset, 0_b, "uart");
	txStream.data = (*bus.writeData)(0, 8_b);
	txStream.valid = *bus.write;
	
	bus.readData = zext(cat(txStream.ready, rxStream.valid, rxStream.data));
	bus.readDataValid = reg(*bus.read, '0');
	rxStream.ready = *bus.readDataValid;
	return config.send(txStream);
}

gtry::Bit gtry::scl::riscv::EmbeddedSystemBuilder::addUART(uint64_t offset, size_t baudRate, const Bit& rx)
{
	UART uart;
	uart.baudRate = (unsigned)baudRate;
	return addUART(offset, uart, rx);
}

bool gtry::scl::riscv::EmbeddedSystemBuilder::BusWindow::overlap(const BusWindow& o) const
{
	bool safe = true;
	if(offset + size > o.offset)
		safe = offset >= o.offset + o.size;
	else if(offset < o.offset)
		safe = offset + size <= o.offset;
	return !safe;
}

gtry::scl::AvalonMM gtry::scl::riscv::EmbeddedSystemBuilder::addAvalonMemMapped(uint64_t offset, BitWidth addrWidth, std::string name)
{
	// check for conflicts
	BusWindow w{
		.offset = offset,
		.size = addrWidth.count(),
		.name = name
	};

	for(const BusWindow& o : m_dataBusWindows)
		HCL_DESIGNCHECK_HINT(!w.overlap(o), "data bus address conflict between " + w.name + " and " + o.name);
	m_dataBusWindows.push_back(w);

	auto ent = m_area.enter((boost::format("avmm_slave_%x_%x") % offset % (offset + addrWidth.count())).str());

	Bit selected = m_dataBus.address(addrWidth.bits(), 32_b - addrWidth.bits()) == (offset >> addrWidth.bits());
	HCL_NAMED(selected);

	m_anyDeviceSelected |= selected;

	AvalonMM ret;
	ret.address = m_dataBus.address(0, addrWidth);
	ret.read = selected & *m_dataBus.read;
	ret.write = selected & *m_dataBus.write;
	ret.writeData = *m_dataBus.writeData;
	ret.byteEnable = *m_dataBus.byteEnable;
	ret.readLatency = 1;

	ret.readDataValid = Bit{};
	ret.readData = m_dataBus.readData->width();

	IF(*ret.readDataValid)
	{
		m_dataBus.readDataValid = '1';
		m_dataBus.readData = *ret.readData;
	}
	ret.setName("avmm");
	return ret;
}

EmbeddedSystemBuilder::Segment gtry::scl::riscv::EmbeddedSystemBuilder::loadSegment(ElfLoader::MegaSegment seg, BitWidth additionalMemSize) const
{
	Segment ret;
	ret.offset = seg.offset;
	ret.size = (seg.size + additionalMemSize).nextPow2();
	ret.addrWidth = BitWidth::count(ret.size.bytes());
	ret.start = ret.offset & ret.addrWidth.mask();
	ret.offset -= ret.start;

	seg.size = ret.size;
	ret.resetState = stateRotateRight(seg.memoryState(), BitWidth{ ret.start * 8 });
	return ret;
}

void gtry::scl::riscv::EmbeddedSystemBuilder::addDataMemory(const ElfLoader& elf, BitWidth scratchMemSize)
{
	ElfLoader::MegaSegment rwMega = elf.segments(6, 0, 1);
	ElfLoader::MegaSegment roMega = elf.segments(4, 0, 3);
	
#if 0 // we added configurable bram reset. do we still want the cpu to init memory?
	std::list<ElfLoader::Segment> newSegments;
	for (const ElfLoader::Segment* seg : rwMega.subSections)
	{
		// make a copy for each data member
		ElfLoader::Segment& seg_no_bss = newSegments.emplace_back(*seg);
		seg_no_bss.size = BitWidth{ seg->data.size() * 8 }; // strip zero area
		seg_no_bss.offset = roMega.offset + roMega.size.bytes();
		seg_no_bss.flags &= ~2;

		roMega.size = roMega.size + seg->size;
		roMega.subSections.push_back(&seg_no_bss);

		// generate initialization code
		assembler::genMeminit(
			seg->offset, seg->offset + seg->size.bytes(),
			seg_no_bss.offset, seg_no_bss.offset + seg_no_bss.size.bytes(),
			m_initCode
		);
	}
#endif

	EmbeddedSystemBuilder::Segment rwSeg = loadSegment(rwMega, scratchMemSize);
	
	uint64_t stackPointer = rwSeg.offset + rwSeg.start + rwSeg.size.bytes();
	assembler::loadConstant((uint32_t)stackPointer, 2, m_initCode);

	addDataMemory(rwSeg, "rw_data", true);
	addDataMemory(loadSegment(roMega, 0_b), "ro_data", false);
}

gtry::hlim::NodeGroup* dbg_group = nullptr;

void gtry::scl::riscv::EmbeddedSystemBuilder::addDataMemory(const Segment& seg, std::string_view name, bool writable)
{
	auto ent = m_area.enter(name);
	if(name == "rw_data")
		dbg_group = GroupScope::getCurrentNodeGroup();

	Memory<UInt> mem{ seg.addrWidth.count(), 32_b };
	mem.setType(MemType::DONT_CARE, 1);
	//mem.noConflicts();
	mem.fillPowerOnState(seg.resetState);
	mem.setName(std::string{ name });

	// alias memory in consecutive address space for ring buffer trick
	AvalonMM bus1 = addAvalonMemMapped(seg.offset, seg.addrWidth, std::string(name) + "_lower");
	AvalonMM bus2 = addAvalonMemMapped(seg.offset + seg.size.bytes(), seg.addrWidth, std::string(name) + "_upper");
	bus1.setName("bus1");
	bus2.setName("bus2");

	UInt addr = bus1.address(2, seg.addrWidth - 2);
	UInt data = mem[addr];

	bus1.readData = reg(data, {.allowRetimingBackward = true});
	bus2.readData = *bus1.readData;
	bus1.readDataValid = reg(*bus1.read, '0');
	bus2.readDataValid = reg(*bus2.read, '0');

	if (writable)
	{
#if 1
		UInt maskedData = data;

		UInt dbgReadData = reg(maskedData, { .allowRetimingBackward = true });
		HCL_NAMED(dbgReadData);

		// TODO: implement byte enable
		for (size_t i = 0; i < 4; ++i)
			IF((*bus1.byteEnable)[i])
				maskedData(i * 8, 8_b) = (*bus1.writeData)(i * 8, 8_b);
		//setName(data, "mem_writedata");

		UInt dbgMaskedData = reg(maskedData, { .allowRetimingBackward = true });
		HCL_NAMED(dbgMaskedData);

		IF(*bus1.write | *bus2.write)
			mem[addr] = maskedData;
#else

		Bit doWrite = reg(*bus1.write | *bus2.write, '0');

		UInt writeAddr = reg(addr);
		UInt writeMask = reg(*bus1.byteEnable);
		UInt writeData = reg(*bus1.writeData);
		UInt maskedData = *bus1.readData;
		for (size_t i = 0; i < 4; ++i)
			IF(writeMask[i])
			maskedData(i * 8, 8_b) = writeData(i * 8, 8_b);

		HCL_NAMED(doWrite);
		HCL_NAMED(writeAddr);
		HCL_NAMED(writeMask);
		HCL_NAMED(writeData);
		HCL_NAMED(maskedData);

		IF(doWrite)
			mem[writeAddr] = maskedData;
#endif
	}
}
