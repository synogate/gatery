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
#include "gatery/pch.h"
#include "EmbeddedSystemBuilder.h"
#include "DualCycleRV.h"

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

	m_dataBus.read = Bit{};
	m_dataBus.write = Bit{};
	m_dataBus.writeData = 32_b;
	m_dataBus.byteEnable = 4_b;
	m_dataBus.readDataValid = Bit{};
	m_dataBus.readData = 32_b;
}

void gtry::scl::riscv::EmbeddedSystemBuilder::addCpu(const ElfLoader& elf, BitWidth scratchMemSize)
{
	auto ent = m_area.enter();

	sim::DefaultBitVectorState codeSection = loadCodeMemState(elf);
	HCL_DESIGNCHECK_HINT(codeSection.size(), "empty code section");

	const BitWidth codeAddrWidth = BitWidth::count(codeSection.size() / 8);
	const uint64_t entryPoint = elf.entryPoint() & codeAddrWidth.mask();

	DualCycleRV rv(codeAddrWidth);
	Memory<BVec>& imem = rv.fetch(entryPoint);
	imem.fillPowerOnState(codeSection);
	
	rv.execute();

	*m_dataBus.readDataValid = reg(*m_dataBus.readDataValid, '0');
	*m_dataBus.readData = reg(*m_dataBus.readData);
	rv.mem(m_dataBus);
	m_dataBus.readData = 0;
	m_dataBus.readDataValid = '0';
	
	m_dataBus.setName("databus");
}

gtry::Bit gtry::scl::riscv::EmbeddedSystemBuilder::addUART(uint64_t offset, UART& config, const Bit& rx)
{
	auto ent = m_area.enter();

	UART::Stream txStream, rxStream = config.recieve(rx);

	txStream.data = (*m_dataBus.writeData)(0, 8_b);
	txStream.valid = *m_dataBus.write & m_dataBus.address == offset;
	
	rxStream.ready = '0';
	IF(m_dataBus.address == offset & *m_dataBus.read)
	{
		m_dataBus.readData = zext(pack(txStream.ready, rxStream.valid, rxStream.data));
		m_dataBus.ready = '1';
	}
	
	return config.send(txStream);
}

gtry::Bit gtry::scl::riscv::EmbeddedSystemBuilder::addUART(uint64_t offset, size_t baudRate, const Bit& rx)
{
	UART uart;
	uart.baudRate = baudRate;
	return addUART(offset, uart, rx);
}

gtry::sim::DefaultBitVectorState gtry::scl::riscv::EmbeddedSystemBuilder::loadCodeMemState(const ElfLoader& elf) const
{
	ElfLoader::MegaSection codeSection = elf.sections(1, 0, 0);

	// TODO: insert init code
	codeSection.size = codeSection.size.nextPow2();

	const BitWidth codeAddrWidth = BitWidth::count(codeSection.size.bytes());
	const BitWidth codeOffset{ (codeSection.offset & codeAddrWidth.mask()) * 8 };
	return stateRotateRight(codeSection.memoryState(), codeOffset);
}
