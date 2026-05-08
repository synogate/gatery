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
	using namespace std::literals;

	template<typename Base>
	class ShiftUnitSequential : public Base
	{
	public:
		ShiftUnitSequential()
		{
			Base::addInstr(InstrDef{ .name = "SH",	.opcode = "01100", .func3 = "-01", .encoding = InstrEncoding::R, .sumSrc = {{OperandSource::Reg, OperandSource::Reg}},	.resultSource = "Shift" });
			Base::addInstr(InstrDef{ .name = "SHI",	.opcode = "00100", .func3 = "-01", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}},	.resultSource = "Shift" });
		}

	protected:

		auto shiftUnit()
		{
			return [this](StreamSignal auto&& in)
				{
					Area ent{ "rivet_bitshift", true };

					const OperandsSum& operand = get<OperandsSum>(in);
					UInt valueInitial = (UInt)operand.sum_operand[0] | name("valueInitial");
					UInt amount = (UInt)operand.sum_operand[1](0, 5_b) | name("amount");

					const InstrOpcode& instr = get<InstrOpcode>(in);
					Bit right = instr.instr[14] | name("right");
					Bit arithmetic = instr.instr[30] | name("arithmetic");
					Bit shift = Base::findInstrCondition(instr, Base::instrAction({ "Shift"sv }, {}, &InstrDef::resultSource)).value_or('0') | name("shift");

					UInt counter = amount.width();
					counter = counter + 1;
					IF(!valid(in) | ready(in))
						counter = 0;
					counter = reg(counter) | name("counter");

					UInt value = valueInitial.width();
					value = mux(right, {
						value << 1,
						cat(arithmetic & value.msb(), value.upper(-1_b))
					});
					value = reg(value) | name("value");

					IF(counter == 0)
						value = valueInitial;

					return move(in) |
						Base::attach([&](const ResultCollection& res) {
							ResultCollection ret = res;
							ret.set({ "Shift", value });
							return ret;
						}) |
						strm::stall(valid(in) & shift & counter != amount);
				};
		}

	};
}
