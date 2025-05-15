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
#include "DualCycleRV.h"
#include "DebugVis.h"
#include "../tilelink/tilelink.h"
#include "../tilelink/TileLinkDemux.h"

#include <gatery/debug/DebugInterface.h>

using namespace gtry;
using namespace gtry::scl;
using namespace gtry::scl::riscv;

gtry::scl::riscv::DualCycleRV::DualCycleRV(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	RV32I(instructionAddrWidth, dataAddrWidth)
{

}

gtry::Memory<gtry::UInt>& gtry::scl::riscv::DualCycleRV::fetch(uint64_t entryPoint)
{
	auto ent = m_area.enter();

	UInt addr = m_IP.width();
	UInt instruction = 32_b;
	{
		auto entRV = m_area.enter();

		BitWidth memWidth = m_IP.width() - 2;
		m_instructionMem.setup(memWidth.count(), 32_b);
		m_instructionMem.setType(MemType::MEDIUM, 1);
		m_instructionMem.setName("instruction_memory");

		IF(!m_stall)
			instruction = m_instructionMem[addr(2, memWidth)];
		instruction = reg(instruction, {.allowRetimingBackward=true});

		HCL_NAMED(instruction);
	}
	generate(instruction, '1');
	addr = genInstructionPointer(entryPoint, '1');
	return m_instructionMem;
}

TileLinkUL gtry::scl::riscv::DualCycleRV::fetchTileLink(uint64_t entryPoint)
{
	auto ent = m_area.enter();
	BVec instruction = 32_b;
	HCL_NAMED(instruction);
	Bit instructionValid;
	HCL_NAMED(instructionValid);

	Bit discardNextInstruction;
	HCL_NAMED(discardNextInstruction);

	generate((UInt)instruction, instructionValid & !discardNextInstruction);

	TileLinkUL link;
	tileLinkInit(link, m_IP.width(), 32_b, 2_b, 0_b);

	Bit requestPending = flag(transfer(link.a), transfer(*link.d) & !transfer(link.a));
	discardNextInstruction = flag(m_overrideIPValid & requestPending, transfer(*link.d));

	UInt ip = m_IP.width();
	{
		auto ent = m_area.enter("IP");

		// 1. ip = istruction pointer to fetch next
		// 2. ipDecode = instruction pointer for current decode stage instruction
		// 3. m_IP = instruction pointer of execute stage instruction

		IF(transfer(link.a))
			ip += 4;
		ip = reg(ip, entryPoint);

		m_overrideIP = m_IP.width();
		HCL_NAMED(m_overrideIPValid);
		HCL_NAMED(m_overrideIP);
		IF(m_overrideIPValid)
			ip = m_overrideIP;
		m_overrideIPValid = '0';
		m_overrideIP = ConstUInt(m_IP.width());

		setName(ip, "fetchIP");
		debugVisualizeIP(ip);

		Bit running = reg(Bit{ '1' }, '0');

		UInt ipDecode = m_IP.width();
		IF(transfer(link.a))
			ipDecode = ip;
		ipDecode = reg(ipDecode, 0);
		HCL_NAMED(ipDecode);

		IF(!m_stall & running & instructionValid)
			m_IP = ipDecode;
		m_IP = reg(m_IP, 0);
		HCL_NAMED(m_IP);
	}

	link.a->opcode = (size_t)TileLinkA::Get;
	link.a->param = 0;
	link.a->size = 2;
	link.a->source = 0;
	link.a->address = ip;
	link.a->mask = 0xF;
	link.a->data = ConstBVec(32_b);

	valid(link.a) = '0';
	IF(transfer(*link.d) & !m_stall)
		valid(link.a) = '1';
	IF(!requestPending & !reset() & !m_stall)
		valid(link.a) = '1';

	// instruction is valid for exactly one non stall cycle
	IF(!m_stall)
		instructionValid = '0';

	instructionValid = reg(instructionValid, '0');
	instruction = reg(instruction);
	ready(*link.d) = '1';
	IF(valid(*link.d))
	{
		instruction = (*link.d)->data;
		instructionValid = '1';
	}

	setName(link, "imem");
	return link;
}

void gtry::scl::riscv::DualCycleRV::fetchText(std::span<const std::byte> text, uint64_t entryPoint)
{
	// m_IP is used internally by fetch to size instructio memory
	m_IP.resetNode();
	m_IP = BitWidth::count(text.size());

	Memory<UInt>& mem = fetch(entryPoint);
	mem.fillPowerOnState(sim::createDefaultBitVectorState(text));
}

void gtry::scl::riscv::DualCycleRV::fetchElf(std::filesystem::path elfPath, std::optional<std::filesystem::path> makeDir)
{
	ElfLoader elf = makeElf(move(elfPath), move(makeDir));

	HCL_DESIGNCHECK_HINT(elf.segments().size() == 1, "firmware is limitted to one relocatable executable segment");
	const ElfLoader::Segment& text = elf.segments().front();
	fetchText(as_bytes(text.data), elf.entryPoint() - text.offset);
}

void gtry::scl::riscv::DualCycleRV::writeCallReturnTrace(std::string filename)
{
	auto clk = ClockScope::getClk();
	auto opcode = pinOut(m_instr.opcode).setName("profile_opcode");
	auto rd = pinOut(m_instr.rd).setName("profile_rd");
	auto rs1 = pinOut(m_instr.rs1).setName("profile_rs1");
	auto target = pinOut(m_overrideIP).setName("profile_target");
	auto ip = pinOut(m_IP).setName("profile_ip");
	auto valid = pinOut(!m_discardResult & !m_stall).setName("profile_valid");

	DesignScope::get()->getCircuit().addSimulationProcess([=]()->SimProcess {

		size_t cycle = 0;
		std::ofstream f{ filename.c_str(), std::ofstream::binary};
		f << std::hex;

		while (1)
		{
			co_await AfterClk(clk);
			cycle++;

			//f << cycle << ' ' << (size_t)simu(ip) << '\n';
			if ((simu(opcode) & 29) == 25 && simu(valid) != '0')
			{
				size_t rdV = simu(rd);
				size_t rs1V = simu(rs1);

				bool rs1Hit = rs1V == 5 || rs1V == 1;
				bool rdHit = rdV == 5 || rdV == 1;
				// pop
				if (simu(opcode) == 25)
					if (rs1Hit && (rs1V != rdV || !rdHit))
						f << cycle << ' ' << (size_t)simu(ip) << " R " << (size_t)simu(target) << '\n';

				// push
				if (rdHit)
					f << cycle << ' ' << (size_t)simu(ip) << " C " << (size_t)simu(target) << '\n';
			}
		}
	});
}

void gtry::scl::riscv::DualCycleRV::generate(const UInt& instruction, const Bit& instructionValid)
{
	auto ent = m_area.enter();

	Instruction pre_inst;
	pre_inst.decode(instruction);
	HCL_NAMED(pre_inst);

	Bit running = reg(Bit{ '1' }, '0');

	IF(!m_stall)
		m_discardResult = m_overrideIPValid | !running | !instructionValid;
	m_discardResult = reg(m_discardResult, '1');
	HCL_NAMED(m_discardResult);

	genRegisterFile(pre_inst.rs1, pre_inst.rs2, pre_inst.rd);
	genInstructionDecode(instruction);
	setupAlu();
	//writeCallReturnTrace("call_return.trace");
}

void gtry::scl::riscv::DualCycleRV::setIP(const UInt& ip)
{
	IF(!m_discardResult)
	{
		m_overrideIPValid = '1';
		m_overrideIP = ip(0, m_IP.width());
	}
}

void gtry::scl::riscv::DualCycleRV::genRegisterFile(UInt rs1, UInt rs2, UInt)
{
	auto scope = m_area.enter("register_file");
	HCL_NAMED(m_resultData);
	HCL_NAMED(m_resultValid);
	HCL_NAMED(m_stall);
	HCL_NAMED(rs1);
	HCL_NAMED(rs2);

	// setup register file
	m_rf.setup(32, 32_b);
	m_rf.setType(MemType::MEDIUM);
	m_rf.initZero();
	m_rf.setName("register_file");

	Bit writeRf = m_resultValid & !m_stall & m_instr.rd != 0;
	HCL_NAMED(writeRf);
	IF(writeRf)
		m_rf[m_instr.rd] = m_resultData;

	IF(!m_stall)
	{
		m_r1 = m_rf[rs1];
		m_r2 = m_rf[rs2];
	}
	m_r1 = reg(m_r1, { .allowRetimingBackward = true });
	m_r2 = reg(m_r2, { .allowRetimingBackward = true });
	HCL_NAMED(m_r1);
	HCL_NAMED(m_r2);

	//debugVisualizeRiscVRegisterFile(writeRf, m_instr.rd, m_resultData, rs1, rs2);
}

UInt gtry::scl::riscv::DualCycleRV::genInstructionPointer(uint64_t entryPoint, const Bit& instructionValid)
{
	auto ent = m_area.enter("IP");

	UInt ip = m_IP.width();
	ip = reg(ip, entryPoint);
	setName(ip, "ip_reg");

	Bit running = reg(Bit{ '1' }, '0');
	IF(!m_stall & running & instructionValid)
	{
		m_IP = ip;
		ip += 4;
	}
	m_IP = reg(m_IP, 0);
	HCL_NAMED(m_IP);

	m_overrideIP = m_IP.width();
	IF(m_overrideIPValid)
		ip = m_overrideIP;

	setName(ip, "ip_next");
	debugVisualizeIP(ip);

	HCL_NAMED(m_overrideIPValid);
	HCL_NAMED(m_overrideIP);
	m_overrideIPValid = '0';
	m_overrideIP = 0;

	return ip;
}

void gtry::scl::riscv::DualCycleRV::genInstructionDecode(UInt instruction)
{
	auto ent = m_area.enter("InstructionDecode");
	HCL_NAMED(instruction);

	UInt instruction_execute = 32_b;
	IF(!m_stall)
		instruction_execute = instruction;
	instruction_execute = reg(instruction_execute);
	HCL_NAMED(instruction_execute);

	m_instr.decode(instruction_execute);
	debugVisualizeInstruction(m_instr);
	HCL_NAMED(m_instr);
}

namespace gtry::scl
{
	template<typename T>
	TileLinkUL tileLinkMemory(Memory<T>& mem, TileLinkMemoryConfig cfg)
	{
		BitWidth wordAddrW = BitWidth::count(mem.wordSize().bytes());
		TileLinkUL link = tileLinkSlaveInit(mem.addressWidth() + wordAddrW, mem.wordSize(), cfg.sourceW);

		auto port = mem[link.a->address.upper(-wordAddrW)];

		if (cfg.writePort)
		{
			IF(transfer(link.a) & link.a->isPut())
			{
				BVec newData = (BVec)pack(port.read());
				for (size_t i = 0; i < link.a->mask.size(); ++i)
					IF(link.a->mask[i])
						newData.part(link.a->mask.size(), i) = link.a->data.part(link.a->mask.size(), i);

				T writeData = constructFrom(mem.defaultValue());
				unpack(newData, writeData);
				port = writeData;
			}
		}

		if (cfg.readPort)
		{
			(*link.d)->data = (BVec)pack(port.read());
		}

		size_t latency = cfg.readPort ? mem.readLatencyHint() : 0;
		for (size_t i = 0; i < latency; ++i)
			*link.d = strm::regDownstream(move(*link.d), { .allowRetimingBackward = true });
		return link;
	}
}

void gtry::scl::riscv::embeddedRV32(TileLinkUL&& dataFetch, std::filesystem::path elfPath, EmbeddedRV32Config cfg)
{
	Area ent{ "scl_embeddedRV32", true };
	using namespace gtry::dbg;

	{
		std::ostringstream msg;
		msg << "Data bus address width is " << dataFetch.a->address.width().bits() << "b";
		log(LogMessage() << LogMessage::Anchor(ent.getNodeGroup()) << msg.str());
	}

	ElfLoader elf = makeElf(move(elfPath), move(cfg.makeDir));

	// we need to take special care to map text section to instruction fetch and rodata to data fetch
	const ElfLoader::Section* text = elf.section(".text");
	const ElfLoader::Section* rodata = elf.section(".rodata");
	HCL_DESIGNCHECK_HINT(text, "no text section in embedded rv32 elf");

	// find segment that contains bss, data, got, ...
	const ElfLoader::Segment* data = nullptr;
	for (const ElfLoader::Segment& s : elf.segments())
	{
		if (s.flags == 6) // read write no execute segment
		{
			data = &s;
			break;
		}
		HCL_DESIGNCHECK_HINT(s.flags == 5, "unhandled segment in embedded rv32 elf");
	}

	auto sectionMapOffset = [](size_t offset, size_t size) {
		size_t mask = gtry::utils::nextPow2(offset ^ (offset + size)) - 1;
		return offset & ~mask;
	};

	BitWidth dataAddrW = BitWidth::count(cfg.dataBusOffset + dataFetch.a->address.width().count());
	if(rodata)
		dataAddrW = std::max(dataAddrW, BitWidth::count(rodata->offset + rodata->data.size()));
	if (data)
		dataAddrW = std::max(dataAddrW, BitWidth::count(data->offset + data->size.bytes()));
	if (cfg.stackByteSize)
		dataAddrW = std::max(dataAddrW, BitWidth::count(cfg.stackOffset + cfg.stackByteSize));

	size_t virtTextOffset = sectionMapOffset(text->offset, text->data.size());
	BitWidth instructionMemAddrW = BitWidth::count(text->offset - virtTextOffset + text->data.size());
	riscv::DualCycleRV rv(instructionMemAddrW, dataAddrW);

	{
		std::ostringstream msg;
		msg << "Instructions size " << text->data.size() << " Byte, virtual offset 0x" << std::hex << virtTextOffset << ", physical offset 0x" << std::hex << (text->offset - virtTextOffset);
		log(LogMessage() << LogMessage::Anchor(ent.getNodeGroup()) << msg.str());
	}

	rv.ipOffset(uint32_t(virtTextOffset));
	Memory<UInt>& mem = rv.fetch(elf.entryPoint() - virtTextOffset);

	sim::DefaultBitVectorState content;
	content.resize(mem.size());
	content.insert(sim::createDefaultBitVectorState(as_bytes(text->data)), (text->offset - virtTextOffset) * 8);
	mem.fillPowerOnState(content);

	rv.execute();

	if(rodata || data || cfg.stackByteSize)
	{
		TileLinkDemux<TileLinkUL> tileLinkBridge;
		TileLinkUL internalDataFetch = tileLinkInit<TileLinkUL>(dataAddrW, dataFetch.a->data.width());
		tileLinkBridge.attachSource(internalDataFetch);
		tileLinkBridge.attachSink(move(dataFetch), cfg.dataBusOffset);

		auto mapSection = [&](std::string name, size_t offset, std::span<const std::byte> data, size_t size, bool writePort) {
			Area section_ent{ "elfsection_" + name, true };

			std::ostringstream msg;
			msg << "Map section at range 0x" << std::hex << offset << "-0x" << (offset + size) << " size " << std::dec << size;
			log(LogMessage() << LogMessage::Anchor(section_ent.getNodeGroup()) << msg.str());

			size_t virtOffset = sectionMapOffset(offset, size);

			BitWidth memAddrW = BitWidth::count(offset - virtOffset + size);
			Memory<BVec> dataMem((memAddrW - 2).count(), 32_b);
			dataMem.noConflicts();
			dataMem.setName(name);

			sim::DefaultBitVectorState content;
			content.resize(dataMem.size());
			// section is initialized to zero in places where data is not present
			content.setRange(sim::DefaultConfig::VALUE, (offset - virtOffset) * 8, size * 8, false);
			content.setRange(sim::DefaultConfig::DEFINED, (offset - virtOffset) * 8, size * 8, true);
			content.insert(sim::createDefaultBitVectorState(data), (offset - virtOffset) * 8);
			dataMem.fillPowerOnState(content);

			tileLinkBridge.attachSink(tileLinkMemory(dataMem, { .writePort = writePort }), virtOffset);
		};

		if (cfg.stackByteSize)
			mapSection("stack", cfg.stackOffset, {}, cfg.stackByteSize, true);
		if (rodata)
			mapSection("rodata", rodata->offset, as_bytes(rodata->data), rodata->data.size(), false);
		if (data)
			mapSection("data", data->offset, as_bytes(data->data), data->size.bytes(), true);

		tileLinkBridge.generate();
		internalDataFetch <<= rv.memTLink();
	}
	else
	{
		dataFetch <<= rv.memTLink();
	}
}
