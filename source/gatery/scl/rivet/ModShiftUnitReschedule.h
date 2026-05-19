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
#include "ModResultSelect.h"
#include "ModLoadStore.h"
#include "ModControlFlow.h"
#include "ModRegisterFile.h"
#include "ModShiftUnit.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class ShiftUnitReschedule : public ShiftUnit<Base>
	{
	public:
		ShiftUnitReschedule() : ShiftUnit<Base>(false)
		{
			Base::addInstr(InstrDef{ .name = "SH",	.opcode = "01100", .func3 = "-01", .encoding = InstrEncoding::R, .sumSrc = {{OperandSource::IP, OperandSource::Zero}}, .aluSrc = {{OperandSource::Reg, OperandSource::Reg}},	.resultSource = "Extern", .jmpTarget = "Sum" });
			Base::addInstr(InstrDef{ .name = "SHI",	.opcode = "00100", .func3 = "-01", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::IP, OperandSource::Zero}}, .aluSrc = {{OperandSource::Reg, OperandSource::Imm}},	.resultSource = "Extern", .jmpTarget = "Sum" });
		}

	protected:

		auto shiftUnit()
		{
			return [this](StreamSignal auto&& in)
				{
					Area ent{ "scl_ShiftUnitReschedule", true };

					Stream operation = removeUpstream(in) |
						Base::attach([](const ShiftCtrl& ctrl, const AluOperands& operand) {
							ShiftCtrl out = ctrl;
							out.shiftValue = operand.alu_operand[0];
							out.shiftAmount = (UInt)operand.alu_operand[1](0, 5_b);
							return out;
						}) |
						Base::attach([](const ShiftCtrl& ctrl, const Valid& valid) {
							return Valid{ valid.valid & ctrl.shiftInstr };
						}) | 
						name("operation");

					Bit busy;
					Stream result = move(operation) |
						Base::attach([&](const Valid& valid) { return Valid{ valid.valid & !busy }; }) |
						strm::attach(Ready{}) |
						strm::transform([](const ShiftCtrl& ctrl) {
							UInt shiftRight = shr((UInt)ctrl.shiftValue, ctrl.shiftAmount, ctrl.shiftArithmetic) | name("shr");
							UInt shiftLeft = ((UInt)ctrl.shiftValue << ctrl.shiftAmount) | name("shl");
							return mux(ctrl.shiftRight, { shiftLeft, shiftRight });
						}) |
						strm::regDownstream() |
						name("result");

					ready(result) = valid(operation);
					busy = flag(transfer(operation), transfer(result)) | name("busy");

					IF(valid(result))
						Base::externalResult(*result);

					// TODO: refactor all of this into externalResult utility
					Bit externalReady = reg(valid(result)) & valid(result) | name("externalReady");
					return move(in) |
						Base::attach([&](const JmpCtrl& jmp, const InstrPtr& ip, const MemCtrl& ctrl) {
							JmpCtrl out = jmp;
							out.jmp &= !externalReady;
							return out;
						}) |
						Base::attach([&](const RegCtrl& reg) {
							RegCtrl out = reg;
							out.regWrite &= externalReady | !valid(operation);
							return out;
						});
				};
		}

	private:
		TileLinkU m_link;

		bool m_waitForStore = false;
	};
}

