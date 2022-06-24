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
#include "riscv.h"
#include "../Adder.h"
#include "../utils/OneHot.h"

void gtry::scl::riscv::Instruction::decode(const UInt& inst)
{
	instruction = inst;

	opcode = inst(2, 5);
	rd = inst(7, 5);
	func3 = inst(12, 3);
	rs1 = inst(15, 5);
	rs2 = inst(20, 5);
	func7 = inst(25, 7);

	immI = sext(inst(20, 12));
	immS = sext(cat(inst(25, 7), inst(7, 5)));
	immB = sext(cat(inst.msb(), inst[7], inst(25, 6), inst(8, 4), '0'));
	immU = cat(inst(20, 12), inst(12, 8), "12b0");
	immJ = sext(cat(inst.msb(), inst(12, 8), inst[20], inst(25, 6), inst(21, 4), '0'));

	// for waveform debugging
	name = 0;
	IF(opcode == "b01101")
		name = 'LUI';
	IF(opcode == "b00101")
		name = 'AUIP';
	IF(opcode == "b11011")
		name = 'JAL';
	IF(opcode == "b11001" & func3 == 0)
		name = 'JALR';
	IF(opcode == "b11000" & func3 == 0)
		name = 'BEQ';
	IF(opcode == "b11000" & func3 == 1)
		name = 'BNE';
	IF(opcode == "b11000" & func3 == 4)
		name = 'BLT';
	IF(opcode == "b11000" & func3 == 5)
		name = 'BGE';
	IF(opcode == "b11000" & func3 == 6)
		name = 'BLTU';
	IF(opcode == "b11000" & func3 == 7)
		name = 'BGEU';

	IF(opcode == "b00000" & func3 == 0)
		name = 'LB';
	IF(opcode == "b00000" & func3 == 1)
		name = 'LH';
	IF(opcode == "b00000" & func3 == 2)
		name = 'LW';
	IF(opcode == "b00000" & func3 == 4)
		name = 'LBU';
	IF(opcode == "b00000" & func3 == 5)
		name = 'LHU';

	IF(opcode == "b01000" & func3 == 0)
		name = 'SB';
	IF(opcode == "b01000" & func3 == 1)
		name = 'SH';
	IF(opcode == "b01000" & func3 == 2)
		name = 'SW';

	IF(opcode == "b00100" & func3 == 0)
		name = 'ADDI';
	IF(opcode == "b00100" & func3 == 1)
		name = 'SLLI';
	IF(opcode == "b00100" & func3 == 2)
		name = 'SLTI';
	IF(opcode == "b00100" & func3 == 3)
		name = 'SLTU';
	IF(opcode == "b00100" & func3 == 4)
		name = 'XORI';
	IF(opcode == "b00100" & func3 == 5)
		name = 'SRLI';
	IF(opcode == "b00100" & func3 == 5 & func7[5])
		name = 'SRAI';
	IF(opcode == "b00100" & func3 == 6)
		name = 'ORI';
	IF(opcode == "b00100" & func3 == 7)
		name = 'ANDI';
	IF(opcode == "b00100" & rd == 0)
		name = 'NOOP';

	IF(opcode == "b01100" & func3 == 0)
		name = 'ADD';
	IF(opcode == "b01100" & func3 == 0 & func7[5])
		name = 'SUB';
	IF(opcode == "b01100" & func3 == 1)
		name = 'SLL';
	IF(opcode == "b01100" & func3 == 2)
		name = 'SLT';
	IF(opcode == "b01100" & func3 == 3)
		name = 'SLTU';
	IF(opcode == "b01100" & func3 == 4)
		name = 'XOR';
	IF(opcode == "b01100" & func3 == 5)
		name = 'SRL';
	IF(opcode == "b01100" & func3 == 5 & func7[5])
		name = 'SRA';
	IF(opcode == "b01100" & func3 == 6)
		name = 'ORI';
	IF(opcode == "b01100" & func3 == 7)
		name = 'AND';
	IF(opcode == "b01100" & rd == 0)
		name = 'NOOP';

	IF(opcode == "b00011")
		name = 'FENC';
	IF(opcode == "b11100")
		name = 'ESYS';
}

gtry::scl::riscv::RV32I::RV32I(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	m_area{"rv32i", true},
	m_IP(instructionAddrWidth),
	m_dataAddrWidth(dataAddrWidth)
{
	m_IPnext = m_IP + 4;
	m_area.leave();
}

void gtry::scl::riscv::RV32I::execute()
{
	auto entRV = m_area.enter("execute");

	m_trace.name = m_area.getNodeGroup()->instancePath();
	m_trace.instructionValid = !m_stall & m_instructionValid;
	m_trace.instruction = m_instr.instruction;
	m_trace.instructionPointer = zext(m_IP) | m_IPoffset;
	m_trace.regWriteValid = m_resultValid & !m_stall & m_instr.rd != 0;
	m_trace.regWriteData = m_resultData;
	m_trace.regWriteAddress = m_instr.rd;
	m_trace.memWriteValid = '0';

	HCL_NAMED(m_resultData);
	HCL_NAMED(m_resultValid);
	HCL_NAMED(m_stall);
	m_resultData = 0;
	m_resultValid = '0';
	m_stall = '0';

	lui();
	auipc();
	jal();
	branch();
	arith();
	logic();
	setcmp();
	shift();
}

void gtry::scl::riscv::RV32I::lui()
{
	IF(m_instr.opcode == "b01101")
		setResult(m_instr.immU);
}

void gtry::scl::riscv::RV32I::auipc()
{
	IF(m_instr.opcode == "b00101")
	{
		auto entAuipc = Area{"auipc"}.enter();
		setResult(m_instr.immU + zext(m_IP) | m_IPoffset);
	}
}

void gtry::scl::riscv::RV32I::jal()
{
	// JAL
	IF(m_instr.opcode == "b11011")
	{
		auto ent = Area{ "jal" }.enter();

		setResult(zext(m_IPnext) | m_IPoffset);
		setIP(zext(m_IP) + m_instr.immJ);
	}

	// JALR
	IF(m_instr.opcode == "b11001")
	{
		auto ent = Area{ "jalr" }.enter();
		m_alu.op2 = m_instr.immI;

		setResult(zext(m_IPnext) | m_IPoffset);
		setIP(m_aluResult.sum);
	}
}

void gtry::scl::riscv::RV32I::branch()
{
	IF(m_instr.opcode == "b11000")
	{
		auto ent = Area{ "branch" }.enter();
		UInt target = m_IP + m_instr.immB(0, m_IP.width());
		
		m_alu.sub = '1';

		// equal
		IF(m_instr.func3 == "b000" & m_aluResult.zero)
			setIP(target);

		// not equal
		IF(m_instr.func3 == "b001" & !m_aluResult.zero)
			setIP(target);

		// less than
		IF(m_instr.func3 == "b100" & m_aluResult.sign != m_aluResult.overflow)
			setIP(target);

		// greater than or equal
		IF(m_instr.func3 == "b101" & m_aluResult.sign == m_aluResult.overflow)
			setIP(target);

		// less than unsigned
		IF(m_instr.func3 == "b110" & !m_aluResult.carry)
			setIP(target);

		// greater than or equal unsigned
		IF(m_instr.func3 == "b111" & m_aluResult.carry)
			setIP(target);
	}
}

void gtry::scl::riscv::RV32I::arith()
{
	IF(m_instr.func3 == 0)
	{
		auto ent = Area{ "arith" }.enter();

		IF(m_instr.opcode == "b00100")
		{
			m_alu.op2 = m_instr.immI;
			setResult(m_aluResult.sum);
		}

		IF(m_instr.opcode == "b01100")
		{
			m_alu.sub = m_instr.func7[5];
			setResult(m_aluResult.sum);
		}
	}
}

void gtry::scl::riscv::RV32I::logic()
{
	IF(m_instr.opcode[4] == '0' & m_instr.opcode(0, 3) == "b100")
	{
		auto ent = Area{ "logic" }.enter();

		UInt op2 = m_instr.immI;
		IF(m_instr.opcode[3])
			op2 = m_r2;

		IF(m_instr.func3 == 4)
			setResult(m_r1 ^ op2);
		IF(m_instr.func3 == 6)
			setResult(m_r1 | op2);
		IF(m_instr.func3 == 7)
			setResult(m_r1 & op2);
	}
}

void gtry::scl::riscv::RV32I::setcmp()
{
	IF(m_instr.opcode[4] == '0' & m_instr.opcode(0,3_b) == "b100" & m_instr.func3(1, 2) == "b01")
	{
		auto ent = Area{ "setcmp" }.enter();

		IF(m_instr.opcode[3] == '0')
			m_alu.op2 = m_instr.immI;
		m_alu.sub = '1';

		Bit lt = !m_aluResult.carry; // unsigned
		IF(m_instr.func3.lsb() == '0')
			lt = m_aluResult.sign != m_aluResult.overflow;

		setResult(zext(lt));
	}
}

void gtry::scl::riscv::RV32I::shift()
{
	IF(m_instr.opcode[4] == '0' & m_instr.opcode(0, 3_b) == "b100" & m_instr.func3(0, 2) == "b01")
	{
		auto ent = Area{ "shift" }.enter();

		UInt amount = m_r2(0, 5_b);
		IF(m_instr.opcode[3] == '0')
			amount = m_instr.immI(0, 5_b);

		UInt number = m_r1;

		Bit left = !m_instr.func3[2];
		IF(left)
			number = swapEndian(number, 1_b);

		Bit arithmetic = m_instr.func7[5];
		number = shr(number, amount, arithmetic);

		IF(left)
			number = swapEndian(number, 1_b);

		setResult(number);
	}
}

void gtry::scl::riscv::RV32I::mem(AvalonMM& mem, bool byte, bool halfword)
{
	auto entRV = m_area.enter();

	mem.read = '0';
	mem.write = '0';
	mem.address = m_aluResult.sum(0, m_dataAddrWidth);
	mem.address(0, 2_b) = 0;
	mem.writeData = m_r2;
	mem.byteEnable = "b1111";

	// check for unaligned access
	Bit is_access = (m_instr.opcode == "b00000" | m_instr.opcode == "b01000") & m_instructionValid;
	UInt access_width = m_instr.func3(0, 2_b);
	sim_assert(!(is_access & access_width == 2) | m_aluResult.sum(0, 2_b) == 0) << "Unaligned access when loading 32 bit word: is_access " << is_access << " access_width: " << access_width << " m_aluResult.sum: " << m_aluResult.sum;
	sim_assert(!(is_access & access_width == 1) | m_aluResult.sum(0, 1_b) == 0) << "Unaligned access when loading 16 bit word: is_access " << is_access << " access_width: " << access_width << " m_aluResult.sum: " << m_aluResult.sum;

	store(mem, byte, halfword);
	load(mem, byte, halfword);
}

void gtry::scl::riscv::RV32I::store(AvalonMM& mem, bool byte, bool halfword)
{
	IF(m_instr.opcode == "b01000" & m_instructionValid)
	{
		auto ent = Area{ "store" }.enter();

		m_alu.op2 = m_instr.immS;
		mem.write = '1';

		if (byte)
		{
			IF(m_instr.func3 == 0)
			{
				mem.writeData = cat(m_r2(0, 8_b), m_r2(0, 8_b), m_r2(0, 8_b), m_r2(0, 8_b));
				mem.byteEnable = decoder(m_aluResult.sum(0, 2_b));
			}
		}
		if (halfword)
		{
			IF(m_instr.func3 == 1)
			{
				mem.writeData = cat(m_r2(0, 16_b), m_r2(0, 16_b));

				Bit highWord = m_aluResult.sum[1];
				mem.byteEnable = cat(highWord, highWord, !highWord, !highWord);
			}
		}
		mem.setName("store_");

	}

	m_trace.memWriteValid = *mem.write;
	m_trace.memWriteAddress = mem.address;
	m_trace.memWriteData = *mem.writeData;
	m_trace.memWriteByteEnable = *mem.byteEnable;
}

void gtry::scl::riscv::RV32I::load(AvalonMM& mem, bool byte, bool halfword)
{
	IF(m_instr.opcode == "b00000" & m_instructionValid)
	{
		auto ent = Area{ "load" }.enter();

		m_alu.op2 = m_instr.immI;

		Bit readStallState;
		readStallState = reg(readStallState, '0');
		HCL_NAMED(readStallState);

		IF(!readStallState)
			mem.read = '1';

		IF(*mem.read)
			readStallState = '1';

		mem.createReadDataValid();
		IF(*mem.readDataValid)
			readStallState = '0';
		setStall(!*mem.readDataValid);

		UInt value = *mem.readData;

		// LB, LBU, LH, LHU
		UInt offset = m_aluResult.sum(0, 2_b);
		UInt type = m_instr.func3(0, 2_b);
		Bit zero = m_instr.func3.msb();
		if (byte)
		{
			IF(type == 0) // byte load
			{
				UInt byte = muxWord(offset, value);
				IF(zero)
					value = zext(byte);
				ELSE
					value = sext(byte);

				mem.byteEnable = decoder(m_aluResult.sum(0, 2_b));
			}
		}
		if (halfword)
		{
			IF(type == 1) // word load
			{
				UInt word = muxWord(offset[1], value);
				IF(zero)
					value = zext(word);
				ELSE
					value = sext(word);

				Bit highWord = m_aluResult.sum[1];
				mem.byteEnable = cat(highWord, highWord, !highWord, !highWord);
			}
		}

		IF(*mem.readDataValid)
			setResult(value);
	}
}

void gtry::scl::riscv::RV32I::setupAlu()
{
	auto ent = m_area.enter();

	// int alu
	HCL_NAMED(m_alu);
	m_alu.result(m_aluResult);
	HCL_NAMED(m_aluResult);

	m_alu.op1 = m_r1;
	m_alu.op2 = m_r2;
	m_alu.sub = '0';
}

void gtry::scl::riscv::IntAluCtrl::result(IntAluResult& out) const
{
	auto ent = Area{ "IntAlu" }.enter();

	auto [sum, carry] = add(op1, op2 ^ sub, sub);

	out.sum = sum;
	out.carry = carry.msb();

	out.zero = op1 == op2;
	out.sign = sum.msb();

	out.overflow = carry[carry.size() - 2] ^ carry.msb();

	setName(out, "alu_result");
}

gtry::scl::riscv::SingleCycleI::SingleCycleI(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	RV32I(instructionAddrWidth, dataAddrWidth),
	m_resultIP(instructionAddrWidth)
{
	m_instructionValid = '1';
}

gtry::Memory<gtry::UInt>& gtry::scl::riscv::SingleCycleI::fetch(uint32_t firstInstructionAddr)
{
	auto entRV = m_area.enter("fetch");

	BitWidth memWidth = m_IP.width() - 2;
	m_instructionMem.setup(memWidth.count(), 32_b);
	m_instructionMem.setType(MemType::SMALL);

	UInt addr = m_IP.width();
	UInt instruction = reg(m_instructionMem[addr(2, memWidth)].read());
	//UInt instruction = m_instructionMem[reg(addr(2, memWidth))];

	Bit firstInstr = reg(Bit{ '0' }, '1');
	HCL_NAMED(firstInstr);
	IF(firstInstr) // noop first instruction
	{
		setStall('1');
		instruction = 0x13;
	}

	HCL_NAMED(instruction);
	addr = fetch(instruction, firstInstructionAddr);

	return m_instructionMem;
}

gtry::UInt gtry::scl::riscv::SingleCycleI::fetch(const UInt& instruction, uint32_t firstInstructionAddr)
{
	m_instr.decode(instruction);
	HCL_NAMED(m_instr);

	IF(!m_stall)
		m_IP = m_resultIP;

	UInt ifetchAddr = m_IP;
	HCL_NAMED(ifetchAddr);

	m_IP = reg(m_IP, firstInstructionAddr);
	HCL_NAMED(m_IP);
	HCL_NAMED(m_resultIP);
	m_resultIP = m_IPnext;
	return ifetchAddr;
}

void gtry::scl::riscv::SingleCycleI::fetchOperands(BitWidth regAddrWidth)
{
	{
		auto entRV = m_area.enter("fetchOperands1");

		m_rf.setup(regAddrWidth.count(), 32_b);
		m_rf.initZero();

		m_r1 = m_rf[m_instr.rs1(0, regAddrWidth)];
		m_r2 = m_rf[m_instr.rs2(0, regAddrWidth)];
		HCL_NAMED(m_r1);
		HCL_NAMED(m_r2);
	}
	setName(m_r1, "r1");
	setName(m_r2, "r2"); // work around for out port used as instance input

	{
		auto entRV = m_area.enter("fetchOperands2");
		setupAlu();

		// this should move into write back (requires write before read policy)
		HCL_NAMED(m_resultData);
		HCL_NAMED(m_resultValid);
		HCL_NAMED(m_stall);
		IF(m_resultValid & !m_stall & m_instr.rd != 0)
			m_rf[m_instr.rd(0, regAddrWidth)] = m_resultData;

		m_resultData = 0;
		m_resultValid = '0';
		m_stall = '0';
	}
}


void gtry::scl::riscv::SingleCycleI::setIP(const UInt& ip)
{
	m_resultIP = ip(0, m_IP.width());
}

void gtry::scl::riscv::RV32I::setResult(const UInt& result)
{
	IF(m_instructionValid)
		m_resultValid = '1';
	m_resultData = zext(result);
}

void gtry::scl::riscv::RV32I::setStall(const Bit& wait)
{
	m_stall |= wait;
}
