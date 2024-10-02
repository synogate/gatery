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
#include "RiscVAssembler.h"

#include "external/riscv-disas.h"

using namespace gtry::scl::riscv::assembler;

void gtry::scl::riscv::assembler::printCode(std::ostream& s, std::span<const uint32_t> code, uint64_t offset)
{
	for(size_t i = 0; i < code.size(); ++i)
	{
		std::string buf;
		disasm_inst(buf, rv32, offset + i * 4, code[i]);
		s << "0x" << std::hex << i*4 << '\t' << buf << '\n';
	}
}

uint32_t gtry::scl::riscv::assembler::typeR(op opcode, func func3, size_t rd, size_t rs1, size_t rs2, size_t func7)
{
	uint32_t icode = (uint32_t)opcode;
	icode |= uint32_t(rd) << 7;
	icode |= uint32_t(func3) << 12;
	icode |= uint32_t(rs1) << 15;
	icode |= uint32_t(rs2) << 20;
	icode |= uint32_t(func7) << 25;
	return icode;
}

uint32_t gtry::scl::riscv::assembler::typeI(op opcode, func func3, size_t rd, size_t rs1, int32_t imm)
{
	uint32_t icode = (uint32_t)opcode;
	icode |= uint32_t(rd) << 7;
	icode |= uint32_t(func3) << 12;
	icode |= uint32_t(rs1) << 15;
	icode |= uint32_t(imm) << 20;
	return icode;
}

uint32_t gtry::scl::riscv::assembler::typeU(op opcode, size_t rd, uint32_t imm)
{
	uint32_t icode = (uint32_t)opcode;
	icode |= uint32_t(rd) << 7;
	icode |= uint32_t(imm);
	return icode;
}

uint32_t gtry::scl::riscv::assembler::typeJ(op opcode, size_t rd, int32_t imm)
{
	uint32_t icode = (uint32_t)opcode;
	icode |= uint32_t(rd) << 7;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 12, 8) << 12;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 11, 1) << 20;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 1, 10) << 21;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 20, 1) << 31;
	return icode;
}

uint32_t gtry::scl::riscv::assembler::typeB(op opcode, func func3, int32_t imm, size_t rs1, size_t rs2)
{
	uint32_t icode = (uint32_t)opcode;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 11, 1) << 7;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 1, 4) << 8;
	icode |= uint32_t(func3) << 12;
	icode |= uint32_t(rs1) << 15;
	icode |= uint32_t(rs2) << 20;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 5, 6) << 25;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 12, 1) << 31;
	return icode;
}

uint32_t gtry::scl::riscv::assembler::typeS(op opcode, func func3, int32_t imm, size_t rs1, size_t rs2)
{
	uint32_t icode = (uint32_t)opcode;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 0, 5) << 7;
	icode |= uint32_t(func3) << 12;
	icode |= uint32_t(rs1) << 15;
	icode |= uint32_t(rs2) << 20;
	icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 5, 7) << 25;
	return icode;
}

uint32_t gtry::scl::riscv::assembler::lui(size_t rd, ImmU imm) { return typeU(op::LUI, rd, imm); }

uint32_t gtry::scl::riscv::assembler::auipc(size_t rd, ImmU imm) { return typeU(op::AUIPC, rd, imm); }

uint32_t gtry::scl::riscv::assembler::jal(size_t rd, ImmJ imm) { return typeJ(op::JAL, rd, imm); }

uint32_t gtry::scl::riscv::assembler::jalr(size_t rd, size_t rs1, ImmI imm) { return typeI(op::JALR, func(0), rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::beq(size_t rs1, size_t rs2, ImmB imm) { return typeB(op::BRANCH, func::BEQ, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::bne(size_t rs1, size_t rs2, ImmB imm) { return typeB(op::BRANCH, func::BNE, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::blt(size_t rs1, size_t rs2, ImmB imm) { return typeB(op::BRANCH, func::BLT, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::bge(size_t rs1, size_t rs2, ImmB imm) { return typeB(op::BRANCH, func::BGE, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::bltu(size_t rs1, size_t rs2, ImmB imm) { return typeB(op::BRANCH, func::BLTU, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::bgeu(size_t rs1, size_t rs2, ImmB imm) { return typeB(op::BRANCH, func::BGEU, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::lb(size_t rd, size_t rs1, ImmI imm) { return typeI(op::LOAD, func::BYTE, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::lbu(size_t rd, size_t rs1, ImmI imm) { return typeI(op::LOAD, func::BYTEU, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::lh(size_t rd, size_t rs1, ImmI imm) { return typeI(op::LOAD, func::HALFWORD, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::lhu(size_t rd, size_t rs1, ImmI imm) { return typeI(op::LOAD, func::HALFWORDU, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::lw(size_t rd, size_t rs1, ImmI imm) { return typeI(op::LOAD, func::WORD, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::sb(size_t rs1, size_t rs2, ImmS imm) { return typeS(op::STORE, func::BYTE, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::sh(size_t rs1, size_t rs2, ImmS imm) { return typeS(op::STORE, func::HALFWORD, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::sw(size_t rs1, size_t rs2, ImmS imm) { return typeS(op::STORE, func::WORD, imm, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::addi(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::ADD, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::slti(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::SLT, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::sltui(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::SLTU, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::xori(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::XOR, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::ori(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::OR, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::andi(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::AND, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::slli(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::SLL, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::srli(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::SRL, rd, rs1, imm); }

uint32_t gtry::scl::riscv::assembler::srai(size_t rd, size_t rs1, ImmI imm) { return typeI(op::ARITHI, func::SRL, rd, rs1, imm.value | 1024); }

uint32_t gtry::scl::riscv::assembler::add(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::ADD, rd, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::sub(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::ADD, rd, rs1, rs2, 32); }

uint32_t gtry::scl::riscv::assembler::slt(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::SLT, rd, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::sltu(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::SLTU, rd, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::xor_(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::XOR, rd, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::or_(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::OR, rd, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::and_(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::AND, rd, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::sll(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::SLL, rd, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::srl(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::SRL, rd, rs1, rs2); }

uint32_t gtry::scl::riscv::assembler::sra(size_t rd, size_t rs1, size_t rs2) { return typeR(op::ARITH, func::SRL, rd, rs1, rs2, 32); }

void gtry::scl::riscv::assembler::loadConstant(uint32_t value, size_t rd, std::vector<uint32_t>& out)
{
	if (value < 2048)
	{
		out.push_back(addi(rd, 0, value));
	}
	else if ((value & 0xFFF) == 0)
	{
		out.push_back(lui(rd, value));
	}
	else if ((value & 0xFFFFF800u) == 0xFFFFF800u)
	{
		out.push_back(addi(rd, 0, value));
	}
	else
	{
		int32_t m = int32_t(value << 20) >> 20; // sign extent immI
		uint32_t k = (value - m) & ~0xFFF;
		out.push_back(lui(rd, k));
		out.push_back(addi(rd, rd, m));
	}
}

void gtry::scl::riscv::assembler::genMeminit(uint64_t dst, uint64_t dstEnd, uint64_t src, uint64_t srcEnd, std::vector<uint32_t>& out, size_t rsBase)
{
	if (src == srcEnd)
		return;
	HCL_ASSERT(dstEnd - dst >= srcEnd - src);

	const size_t rd = rsBase + 0;
	const size_t rde = rsBase + 1;
	const size_t rs = rsBase + 2;
	const size_t rse = rsBase + 3;
	const size_t rtmp = rsBase + 4;

	assembler::loadConstant(uint32_t(dst), rd, out);
	assembler::loadConstant(uint32_t(src), rs, out);
	assembler::loadConstant(uint32_t(srcEnd), rse, out);

	out.push_back(assembler::lbu(rtmp, rs, 0));
	out.push_back(assembler::addi(rs, rs, 1));
	out.push_back(assembler::sb(rd, rtmp, 0));
	out.push_back(assembler::addi(rd, rd, 1));
	out.push_back(assembler::bne(rs, rse, -16));

	if (dstEnd - dst == srcEnd - src)
		return;

	assembler::loadConstant(uint32_t(dstEnd), rde, out);
	out.push_back(assembler::sb(rd, 0, 0));
	out.push_back(assembler::addi(rd, rd, 1));
	out.push_back(assembler::bne(rd, rde, -8));
}
