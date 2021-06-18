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

void gtry::scl::riscv::Instruction::decode(const BVec& inst)
{
	opcode = inst(2, 5);
	rd = inst(7, 5);
	func3 = inst(12, 3);
	rs1 = inst(15, 5);
	rs2 = inst(20, 5);
	func7 = inst(25, 7);

	immI = sext(inst(20, 12));
	immS = sext(pack(inst(25, 7), inst(7, 5)));
	immB = sext(pack(inst.msb(), inst[7], inst(25, 6), inst(8, 4), '0'));
	immU = pack(inst(20, 12), inst(12, 8), "12b0");
	immJ = sext(pack(inst.msb(), inst(12, 8), inst[20], inst(25, 6), inst(21, 4), '0'));
}

gtry::scl::riscv::RV32I::RV32I(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	m_IP(instructionAddrWidth),
	m_dataAddrWidth(dataAddrWidth)
{
	m_IPnext = m_IP + 4;
}

void gtry::scl::riscv::RV32I::execute(AvalonMM& m)
{
	lui();
	auipc();
	jal();
	branch();
	arith();
	logic();
	setcmp();
	shift();
	mem(m);
}

void gtry::scl::riscv::RV32I::lui()
{
	IF(m_instr.opcode == "b01101")
		setResult(m_instr.immU);
}

void gtry::scl::riscv::RV32I::auipc()
{
	IF(m_instr.opcode == "b00101")
		setResult(m_instr.immU + zext(m_IP));
}

void gtry::scl::riscv::RV32I::jal()
{
	// JAL
	IF(m_instr.opcode == "b11011")
	{
		setResult(m_IPnext);
		setIP(zext(m_IP) + m_instr.immJ);
	}

	// JALR
	IF(m_instr.opcode == "b11001")
	{
		m_alu.op2 = m_instr.immI;

		setResult(m_IPnext);
		setIP(m_aluResult.sum);
	}
}

void gtry::scl::riscv::RV32I::branch()
{
	IF(m_instr.opcode == "b11000")
	{
		BVec target = m_IP + m_instr.immB(0, m_IP.getWidth());	
		
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
		BVec op2 = m_instr.immI;
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
		BVec amount = m_r2(0, 5_b);
		IF(m_instr.opcode[3] == '0')
			amount = m_instr.immI(0, 5_b);

		BVec number = m_r1;

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

void gtry::scl::riscv::RV32I::mem(AvalonMM& mem)
{
	mem.read = '0';
	mem.write = '0';
	mem.address = m_aluResult.sum(0, m_dataAddrWidth);
	mem.address(0, 2_b) = 0;
	mem.writeData = m_r2;
	mem.byteEnable = "b1111";

	// store
	IF(m_instr.opcode == "b01000")
	{
		m_alu.op2 = m_instr.immS;
		mem.write = '1';

		IF(m_instr.func3 == 0)
		{
			mem.writeData = pack(m_r2(0, 8_b), m_r2(0, 8_b), m_r2(0, 8_b), m_r2(0, 8_b));
			mem.byteEnable = decoder(m_aluResult.sum(0, 2_b));
		}
		ELSE IF(m_instr.func3 == 1)
		{
			mem.writeData = pack(m_r2(0, 16_b), m_r2(0, 16_b));

			Bit highWord = m_aluResult.sum[1];
			mem.byteEnable = pack(highWord, highWord, !highWord, !highWord);
		}
	}

	// load
	IF(m_instr.opcode == "b00000")
	{
		m_alu.op2 = m_instr.immI;

		Bit readStallState;
		readStallState = reg(readStallState, '0');

		IF(!readStallState)
			mem.read = '1';

		IF(*mem.read)
			readStallState = '1';

		mem.createReadDataValid();
		IF(*mem.readDataValid)
			readStallState = '0';
		setStall(!*mem.readDataValid);

		BVec value = *mem.readData;

		// LB, LBU, LH, LHU
		BVec offset = m_aluResult.sum(0, 2_b);
		BVec type = m_instr.func3(0, 2_b);
		Bit zero = m_instr.func3.msb();
		IF(type == 0) // byte load
		{
			BVec byte = mux(offset, value);
			IF(zero)
				value = zext(byte);
			ELSE
				value = sext(byte);

			mem.byteEnable = decoder(m_aluResult.sum(0, 2_b));
		}
		ELSE IF(type == 1) // word load
		{
			BVec word = mux(offset[1], value);
			IF(zero)
				value = zext(word);
			ELSE
				value = sext(word);

			Bit highWord = m_aluResult.sum[1];
			mem.byteEnable = pack(highWord, highWord, !highWord, !highWord);
		}

		setResult(value);
	}
}

void gtry::scl::riscv::IntAluCtrl::result(IntAluResult& out) const
{
	auto [sum, carry] = add(op1, op2 ^ sub, sub);

	out.sum = sum;
	out.carry = carry.msb();

	out.zero = sum == 0;
	out.sign = sum.msb();

	out.overflow = carry[carry.size() - 2] ^ carry.msb();

	setName(out, "alu_result");
}

gtry::scl::riscv::SingleCycleI::SingleCycleI(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	RV32I(instructionAddrWidth, dataAddrWidth),
	m_resultIP(instructionAddrWidth)
{

}

gtry::Memory<gtry::BVec>& gtry::scl::riscv::SingleCycleI::fetch()
{
	m_instructionMem.setup(m_IP.getWidth().count(), 32_b);

	BVec instruction = 32_b;
	IF(!m_stall)
	{
		instruction = m_instructionMem[m_resultIP >> 2].read();
	}
	instruction = reg(instruction, 0);
	HCL_NAMED(instruction);
	m_instr.decode(instruction);
	HCL_NAMED(m_instr);

	IF(!m_stall)
		m_IP = m_resultIP;

	m_IP = reg(m_IP, 0);
	HCL_NAMED(m_IP);

	HCL_NAMED(m_resultIP);
	m_resultIP = m_IPnext;

	return m_instructionMem;
}

gtry::BVec gtry::scl::riscv::SingleCycleI::fetch(const BVec& instruction)
{
	m_instr.decode(instruction);
	HCL_NAMED(m_instr);
	return m_resultIP;
}

void gtry::scl::riscv::SingleCycleI::fetchOperands(BitWidth regAddrWidth)
{
	m_rf.setup(regAddrWidth.count(), 32_b);
	m_rf.setPowerOnStateZero();

	m_r1 = m_rf[m_instr.rs1(0, regAddrWidth)];
	m_r2 = m_rf[m_instr.rs2(0, regAddrWidth)];
	HCL_NAMED(m_r1);
	HCL_NAMED(m_r2);

	HCL_NAMED(m_alu);
	m_alu.result(m_aluResult);
	HCL_NAMED(m_aluResult);

	m_alu.op1 = m_r1;
	m_alu.op2 = m_r2;
	m_alu.sub = '0';

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


void gtry::scl::riscv::SingleCycleI::setIP(const BVec& ip)
{
	m_resultIP = ip(0, m_IP.getWidth());
}

void gtry::scl::riscv::SingleCycleI::setResult(const BVec& result)
{
	m_resultValid = '1';
	m_resultData = zext(result);
}

void gtry::scl::riscv::SingleCycleI::setStall(const Bit& wait)
{
	m_stall |= wait;
}
