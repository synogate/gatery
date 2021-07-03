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
#include <gatery/utils/BitManipulation.h>
#include <span>

namespace gtry::scl::riscv
{
	namespace assembler
	{
		enum class op
		{
			LUI = 0x37,
			AUIPC = 0x17,
			JAL = 0x6F,
			JALR = 0x67,
			BRANCH = 0x63,
			LOAD = 0x03,
			STORE = 0x23,
			ARITHI = 0x13,
			ARITH = 0x33,
			FENCE = 0x0F,
			SYSTEM = 0x73
		};

		enum class func
		{
			// arith
			ADD = 0,
			SLL = 1,
			SLT = 2,
			SLTU = 3,
			XOR = 4,
			SRL = 5,
			OR = 6,
			AND = 7,

			// branch
			BEQ = 0,
			BNE = 1,
			BLT = 4,
			BGE = 5,
			BLTU = 6,
			BGEU = 7,

			// load/store
			BYTE = 0,
			HALFWORD = 1,
			WORD = 2,
			BYTEU = 4,
			HALFWORDU = 5
		};

		template<typename T, T mask>
		struct Imm
		{
			Imm(T val) : value(val & mask) {}
			operator T () { return value; }
			T value;
		};

		using ImmU = Imm<uint32_t, 0xFFFFF000ull>;
		using ImmJ = Imm<int32_t, 0x1FFFFE>;
		using ImmB = Imm<int32_t, 0x1FFE>;
		using ImmS = Imm<int32_t, 0xFFF>;
		using ImmI = Imm<int32_t, 0xFFF>;

		uint32_t typeR(op opcode, func func3, size_t rd = 0, size_t rs1 = 0, size_t rs2 = 0, size_t func7 = 0);
		uint32_t typeI(op opcode, func func3, size_t rd, size_t rs1, int32_t imm);
		uint32_t typeU(op opcode, size_t rd, uint32_t imm);
		uint32_t typeJ(op opcode, size_t rd, int32_t imm);
		uint32_t typeB(op opcode, func func3, int32_t imm, size_t rs1 = 0, size_t rs2 = 0);
		uint32_t typeS(op opcode, func func3, int32_t imm, size_t rs1 = 0, size_t rs2 = 0);

		uint32_t lui(size_t rd, ImmU imm);
		uint32_t auipc(size_t rd, ImmU imm);
		uint32_t jal(size_t rd, ImmJ imm);
		uint32_t jalr(size_t rd, size_t rs1, ImmI imm);
		uint32_t beq(size_t rs1, size_t rs2, ImmB imm);
		uint32_t bne(size_t rs1, size_t rs2, ImmB imm);
		uint32_t blt(size_t rs1, size_t rs2, ImmB imm);
		uint32_t bge(size_t rs1, size_t rs2, ImmB imm);
		uint32_t bltu(size_t rs1, size_t rs2, ImmB imm);
		uint32_t bgeu(size_t rs1, size_t rs2, ImmB imm);
		uint32_t lb(size_t rd, size_t rs1, ImmI imm);
		uint32_t lbu(size_t rd, size_t rs1, ImmI imm);
		uint32_t lh(size_t rd, size_t rs1, ImmI imm);
		uint32_t lhu(size_t rd, size_t rs1, ImmI imm);
		uint32_t lw(size_t rd, size_t rs1, ImmI imm);
		uint32_t sb(size_t rs1, size_t rs2, ImmS imm);
		uint32_t sh(size_t rs1, size_t rs2, ImmS imm);
		uint32_t sw(size_t rs1, size_t rs2, ImmS imm);
		uint32_t addi(size_t rd, size_t rs1, ImmI imm);
		uint32_t slti(size_t rd, size_t rs1, ImmI imm);
		uint32_t sltui(size_t rd, size_t rs1, ImmI imm);
		uint32_t xori(size_t rd, size_t rs1, ImmI imm);
		uint32_t ori(size_t rd, size_t rs1, ImmI imm);
		uint32_t andi(size_t rd, size_t rs1, ImmI imm);
		uint32_t slli(size_t rd, size_t rs1, ImmI imm);
		uint32_t srli(size_t rd, size_t rs1, ImmI imm);
		uint32_t srai(size_t rd, size_t rs1, ImmI imm);
		uint32_t add(size_t rd, size_t rs1, size_t rs2);
		uint32_t sub(size_t rd, size_t rs1, size_t rs2);
		uint32_t slt(size_t rd, size_t rs1, size_t rs2);
		uint32_t sltu(size_t rd, size_t rs1, size_t rs2);
		uint32_t xor_(size_t rd, size_t rs1, size_t rs2);
		uint32_t or_(size_t rd, size_t rs1, size_t rs2);
		uint32_t and_(size_t rd, size_t rs1, size_t rs2);
		uint32_t sll(size_t rd, size_t rs1, size_t rs2);
		uint32_t srl(size_t rd, size_t rs1, size_t rs2);
		uint32_t sra(size_t rd, size_t rs1, size_t rs2);

		void loadConstant(uint32_t value, size_t rd, std::vector<uint32_t>& out);
		void genMeminit(uint64_t dst, uint64_t dstEnd, uint64_t src, uint64_t srcEnd, std::vector<uint32_t>& out, size_t rsBase = 10);

		void printCode(std::ostream& s, std::span<const uint32_t> code, uint64_t offset);
	};
}
