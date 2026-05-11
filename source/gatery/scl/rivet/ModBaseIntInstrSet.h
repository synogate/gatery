/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2026 Michael Offel, Andreas Ley

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
#include <gatery/frontend.h>
#include "ModInstrSet.h"
#include "ModIntAlu.h"
#include "ModResultSelect.h"

namespace gtry::scl::rivet
{
}

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class BaseIntInstrSet : public Base
	{
	public:
		BaseIntInstrSet()
		{
			Base::addInstr(InstrDef{ .name = "LUI",		.opcode = "01101", .encoding = InstrEncoding::U, .aluSrc = { {OperandSource::DontCare, OperandSource::Imm} },	.aluMode = AluMode::B,				.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "AUIPC",	.opcode = "00101", .encoding = InstrEncoding::U, .aluSrc = { {OperandSource::IP, OperandSource::Imm}},			.aluMode = AluMode::Add,			.resultSource = "Alu" });

			Base::addInstr(InstrDef{ .name = "ADDI",	.opcode = "00100", .func3 = "000", .encoding = InstrEncoding::I, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}},	.aluMode = AluMode::Add,	.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "XORI",	.opcode = "00100", .func3 = "100", .encoding = InstrEncoding::I, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}},	.aluMode = AluMode::Xor,	.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "ORI",		.opcode = "00100", .func3 = "110", .encoding = InstrEncoding::I, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}},	.aluMode = AluMode::Or,		.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "ANDI",	.opcode = "00100", .func3 = "111", .encoding = InstrEncoding::I, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}},	.aluMode = AluMode::And,	.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "SETCMPI",	.opcode = "00100", .func3 = "01-", .encoding = InstrEncoding::I, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}},	.aluMode = AluMode::Less,	.resultSource = "Cmp" });
			
			Base::addInstr(InstrDef{ .name = "ADD",		.opcode = "01100", .func3 = "000", .encoding = InstrEncoding::R, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}},	.aluMode = AluMode::Arith,	.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "XOR",		.opcode = "01100", .func3 = "100", .encoding = InstrEncoding::R, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}},	.aluMode = AluMode::Xor,	.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "OR",		.opcode = "01100", .func3 = "110", .encoding = InstrEncoding::R, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}},	.aluMode = AluMode::Or,		.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "AND",		.opcode = "01100", .func3 = "111", .encoding = InstrEncoding::R, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}},	.aluMode = AluMode::And,	.resultSource = "Alu" });
			Base::addInstr(InstrDef{ .name = "SETCMP",	.opcode = "01100", .func3 = "01-", .encoding = InstrEncoding::R, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}},	.aluMode = AluMode::Less,	.resultSource = "Cmp" });

			Base::addInstr(InstrDef{ .name = "JAL",		.opcode = "11011", .encoding = InstrEncoding::UJ,	.sumSrc = {{OperandSource::IP, OperandSource::Imm}},	.aluSrc = {{OperandSource::IP, OperandSource::InstrSize}},	.aluMode = AluMode::Add, .resultSource = "Alu", .jmpTarget = "Sum" });
			Base::addInstr(InstrDef{ .name = "JALR",	.opcode = "11001", .encoding = InstrEncoding::I,	.sumSrc = {{OperandSource::Reg, OperandSource::Imm}},	.aluSrc = {{OperandSource::IP, OperandSource::InstrSize}},	.aluMode = AluMode::Add, .resultSource = "Alu", .jmpTarget = "Sum" });
			Base::addInstr(InstrDef{ .name = "BRAEQ",	.opcode = "11000", .func3 = "00-", .encoding = InstrEncoding::SB,	.sumSrc = {{OperandSource::IP, OperandSource::Imm}},	.aluSrc = {{OperandSource::Reg, OperandSource::Reg}},		.aluMode = AluMode::Equal,		.jmpTarget = "Sum" });
			Base::addInstr(InstrDef{ .name = "BRALT",	.opcode = "11000", .func3 = "1--", .encoding = InstrEncoding::SB,	.sumSrc = {{OperandSource::IP, OperandSource::Imm}},	.aluSrc = {{OperandSource::Reg, OperandSource::Reg}},		.aluMode = AluMode::Less,		.jmpTarget = "Sum" });
		}

	protected:

		auto selectAluResults()
		{
			return [this](const ResultCollection& in, const InstrOpcode& instr, const AluResult& alu, const SumResult& sum, const SumOperands& op)
				{
					ResultCollection ret = in;
					ret.set({ "Alu", alu.alu_result });
					ret.set({ "Sum", sum.sum_result });

					Bit lt = !alu.alu_carry; // unsigned
					IF(instr.instr[12] == '0')
						lt = alu.alu_sign != alu.alu_overflow; // signed
					ret.set({ "Cmp", zext(lt) });


					return ret;
				};
		}

	};
}
