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
#include "ModShiftUnit.h"

namespace gtry::scl::rivet::mod
{
	using namespace std::literals;

	template<typename Base>
	class ShiftUnitSequential : public ShiftUnit<Base>
	{
	public:
		ShiftUnitSequential() : ShiftUnit<Base>(false)
		{
			Base::addInstr(InstrDef{ .name = "SH",	.opcode = "01100", .func3 = "-01", .encoding = InstrEncoding::R, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}},	.resultSource = "Shift" });
			Base::addInstr(InstrDef{ .name = "SHI",	.opcode = "00100", .func3 = "-01", .encoding = InstrEncoding::I, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}},	.resultSource = "Shift" });
		}

	protected:

		auto shiftUnit()
		{
			return [this](StreamSignal auto&& in)
				{
					Area ent{ "rivet_bitshift", true };

					const AluOperands& operand = get<AluOperands>(in);
					ShiftCtrl ctrl = get<ShiftCtrl>(in);
					ctrl.shiftValue = operand.alu_operand[0];
					ctrl.shiftAmount = (UInt)operand.alu_operand[1](0, 5_b);

					Bit readyForInput = flag(transfer(in), valid(in), '1') | name("readyForInput");

					UInt counter = ctrl.shiftAmount.width();
					counter = counter - 1;
					counter = reg(counter) | name("counter");
					IF(readyForInput)
						counter = ctrl.shiftAmount;

					BVec value = ctrl.shiftValue.width();
					value = mux(ctrl.shiftRight, {
						value << 1,
						(BVec)cat(ctrl.shiftArithmetic & value.msb(), value.upper(-1_b))
					});
					value = reg(value) | name("value");

					IF(readyForInput)
						value = ctrl.shiftValue;

					return move(in) |
						Base::attach([&](const ResultCollection& res) {
							ResultCollection ret = res;
							ret.set({ "Shift", value });
							return ret;
						}) |
						strm::stall(valid(in) & ctrl.shiftInstr & counter != 0);
				};
		}

	};
}
