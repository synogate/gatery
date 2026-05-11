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
#include "ModResultSelect.h"
#include "ModIntAlu.h"
#include "ModInstrFetch.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class ShiftUnit : public Base
	{
	public:
		ShiftUnit()
		{
			Base::addInstr(InstrDef{ .name = "SLL",		.opcode = "01100", .func3 = "001", .encoding = InstrEncoding::R, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}}, .resultSource = "ShiftLeft" });
			Base::addInstr(InstrDef{ .name = "SLLI",	.opcode = "00100", .func3 = "001", .encoding = InstrEncoding::I, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "ShiftLeft" });
			Base::addInstr(InstrDef{ .name = "SR",		.opcode = "01100", .func3 = "101", .encoding = InstrEncoding::R, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}}, .resultSource = "ShiftRight" });
			Base::addInstr(InstrDef{ .name = "SRI",		.opcode = "00100", .func3 = "101", .encoding = InstrEncoding::I, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "ShiftRight" });
		}

	protected:

		auto shiftUnit()
		{
			return [this](StreamSignal auto&& in) {
				return move(in) |
					Base::attach([this](const ResultCollection& in, const AluOperands& operand, const InstrOpcode& instr) { return shift(in, operand, instr); })
					;
				};
		}

		ResultCollection shift(const ResultCollection& in, const AluOperands& operand, const InstrOpcode& instr)
		{
			Area ent{ "rivet_bitshift", true };
			ResultCollection ret = in;

			UInt value = (UInt)operand.alu_operand[0] | name("value");
			UInt amount = (UInt)operand.alu_operand[1](0, 5_b) | name("amount");

			UInt shiftLeft = (value << amount) | name("shl");
			ret.result.push_back({ "ShiftLeft", shiftLeft });

			Bit arithmetic = instr.instr[30] | name("arithmetic");
			UInt shiftRight = shr(value, amount, arithmetic) | name("shr");
			ret.result.push_back({ "ShiftRight", shiftRight });

			return ret;
		};

	};
}
