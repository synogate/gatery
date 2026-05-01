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
			Base::addInstr(InstrDef{ .name = "LUI",		.opcode = "01101", .encoding = InstrEncoding::U, .sumSrc = { {OperandSource::Zero, OperandSource::Imm} },	.resultSource = "Sum" });
			Base::addInstr(InstrDef{ .name = "AUIPC",	.opcode = "00101", .encoding = InstrEncoding::U,	.sumSrc = {{OperandSource::IP, OperandSource::Imm}},	.resultSource = "Sum" });
			Base::addInstr(InstrDef{ .name = "ADD",		.opcode = "01100", .func3 = "000", .encoding = InstrEncoding::R, .sumSrc = {{OperandSource::Reg, OperandSource::Reg}}, .difSrc = {{OperandSource::Reg, OperandSource::Reg}}, .resultSource = "AddInstr" });
			Base::addInstr(InstrDef{ .name = "ADDI",	.opcode = "00100", .func3 = "000", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "Sum" });
			Base::addInstr(InstrDef{ .name = "XOR",		.opcode = "01100", .func3 = "100", .encoding = InstrEncoding::R, .sumSrc = {{OperandSource::Reg, OperandSource::Reg}}, .resultSource = "LogicXor" });
			Base::addInstr(InstrDef{ .name = "XORI",	.opcode = "00100", .func3 = "100", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "LogicXor" });
			Base::addInstr(InstrDef{ .name = "OR",		.opcode = "01100", .func3 = "110", .encoding = InstrEncoding::R, .sumSrc = {{OperandSource::Reg, OperandSource::Reg}}, .resultSource = "LogicOr" });
			Base::addInstr(InstrDef{ .name = "ORI",		.opcode = "00100", .func3 = "110", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "LogicOr" });
			Base::addInstr(InstrDef{ .name = "AND",		.opcode = "01100", .func3 = "111", .encoding = InstrEncoding::R, .sumSrc = {{OperandSource::Reg, OperandSource::Reg}}, .resultSource = "LogicAnd" });
			Base::addInstr(InstrDef{ .name = "ANDI",	.opcode = "00100", .func3 = "111", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "LogicAnd" });
			Base::addInstr(InstrDef{ .name = "SETCMP",	.opcode = "01100", .func3 = "01-", .encoding = InstrEncoding::R, .difSrc = {{OperandSource::Reg, OperandSource::Reg}}, .resultSource = "Cmp" });
			Base::addInstr(InstrDef{ .name = "SETCMPI",	.opcode = "00100", .func3 = "01-", .encoding = InstrEncoding::I, .difSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "Cmp" });

			Base::addInstr(InstrDef{ .name = "JAL",		.opcode = "11011", .encoding = InstrEncoding::UJ,	.sumSrc = {{OperandSource::IP, OperandSource::Imm}},	.difSrc = {{OperandSource::IP, OperandSource::InvInstrSize}},	.resultSource = "Diff", .jmpTarget = "Sum" });
			Base::addInstr(InstrDef{ .name = "JALR",	.opcode = "11001", .encoding = InstrEncoding::I,	.sumSrc = {{OperandSource::Reg, OperandSource::Imm}},	.difSrc = {{OperandSource::IP, OperandSource::InvInstrSize}},	.resultSource = "Diff", .jmpTarget = "Sum" });
			Base::addInstr(InstrDef{ .name = "BRA",		.opcode = "11000", .encoding = InstrEncoding::SB,	.sumSrc = {{OperandSource::IP, OperandSource::Imm}},	.difSrc = {{OperandSource::Reg, OperandSource::Reg}},			.jmpTarget = "Sum" });
		}

	protected:

		auto selectAluResults()
		{
			return [this](const ResultCollection& in, const InstrOpcode& instr, const AluDiff& diff, const AluSum& sum, const OperandsSum& op)
				{
					ResultCollection ret = in;
					ret.set({ "AddInstr", mux(instr.instr[30], { sum.sum, diff.diff }) });
					ret.set({ "Sum", sum.sum });
					ret.set({ "Diff", diff.diff });
					ret.set({ "LogicXor", op.sum_operand[0] ^ op.sum_operand[1] });
					ret.set({ "LogicOr",  op.sum_operand[0] | op.sum_operand[1] });
					ret.set({ "LogicAnd", op.sum_operand[0] & op.sum_operand[1] });

					Bit lt = !diff.diff_carry; // unsigned
					IF(instr.instr[12] == '0')
						lt = diff.diff_sign != diff.diff_overflow;
					ret.set({ "Cmp", zext(lt, Base::dataPathW()) });
					return ret;
				};
		}

	};
}
