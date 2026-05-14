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
#include "../stream/strm.h"
#include "../arch/colognechip/BRAM.h"
#include "../arch/colognechip/lut.h"
#include "../arch/colognechip/RiscVopt.h"
#include "../arch/colognechip/Arithmetic.h"
#include "ModRegisterFile.h"

namespace gtry::scl::rivet::mod
{
	struct GateMateBranchCtrl
	{
		Bit carryInvert;
		Bit enSigned;
		Bit enCarry;
	};

	template<typename Base>
	class GateMate : public Base
	{
	public:

	protected:

#if 1
		auto addJmpCtrl() {
			return [this](const InstrPtr& instr)
				{
					// we handle JAL, JALR as normal branch with carry allways clear
					return JmpCtrl{
						.jmp = '0',
						.jmpTarget = ConstUInt(instr.instrPtr.width()),
					};
				};
		}

		auto branchDecode()
		{
			return [this](const InstrOpcode& instr)
				{
					/*
						BEQ = Carry (our alu outputs carry in equal mode)
						BNE = !Carry
						BLT = Sign ^ Carry ^ Carry-1
						BGE = !(Sign ^ Carry ^ Carry-1)
						BLTU = !Carry
						BGEU = Carry

						We split the branch decision into two pieces. One for the signed contribution and one for everything else:
						((Sign ^ Carry-1) & enSigned) ^ ((Carry ^ carryInvert) & enCarry)

						BEQ = enCarry
						BNE = enCarry, carryInvert
						BLT = enCarry, enSigned
						BGE = enCarry, enSigned, carryInvert
						BLTU= enCarry, carryInvert
						BGEU= enCarry

						JAL and JALR both use the alu to add IP + InstrSize, which we assume never produces a carry.
						We therefore treat them as BNE

						JAL = enCarry, carryInvert
						JALR= enCarry, carryInvert
					*/
					GateMateBranchCtrl ctrl;

					// enSigned
					std::vector<MatchValue> actions;
					actions.reserve(Base::instructions().size());
					for (const InstrDef& def : Base::instructions())
						if (def.name == "BLT" || def.name == "BGE")
							actions.push_back(MatchValue::High);
						else
							actions.push_back(MatchValue::Low);

					ctrl.enSigned = Base::findInstrCondition(instr, actions).value_or('0');

					// enCarry
					actions.clear();
					for (const InstrDef& def : Base::instructions())
						if (!def.jmpTarget.empty())
							actions.push_back(MatchValue::High);
						else
							actions.push_back(MatchValue::Low);

					ctrl.enCarry = Base::findInstrCondition(instr, actions).value_or('0');

					// carryInvert is dontCare when enCarry is low
					actions.clear();
					for (const InstrDef& def : Base::instructions())
						if (def.jmpTarget.empty())
							actions.push_back(MatchValue::DontCare);
						else if (def.name != "BEQ" && def.name != "BLT" && def.name != "BGEU") 
							// inverse condition to include custom added jump target (carry has to be low for this to work)
							actions.push_back(MatchValue::High);
						else
							actions.push_back(MatchValue::Low);

					ctrl.carryInvert = Base::findInstrCondition(instr, actions).value_or('0');

					return ctrl;
				};
		}

		auto branchDecision()
		{
			return [this](const JmpCtrl& in, const AluResult& flags, const GateMateBranchCtrl& ctrl)
				{
					arch::colognechip::CC_L2T4 upper;

					// Sign ^ Carry-1
					upper.l00(0b0110);
					upper.I0() = flags.alu_in_sign[0];
					upper.I1() = flags.alu_in_sign[1];

					// (Sign ^ Carry-1) & enSigned
					upper.l01(0b1010);
					upper.I2() = ctrl.enSigned;
					upper.l10(0b1000);

					upper.updateSim();
					
					arch::colognechip::CC_L2T5 lower;
					lower.I4(upper);

					// Carry ^ carryInvert
					lower.l00(0b0110);
					lower.I0() = flags.alu_carry;
					lower.I1() = ctrl.carryInvert;

					// (Carry ^ carryInvert) & enCarry
					lower.l01(0b1010);
					lower.I2() = ctrl.enCarry;
					lower.l10(0b1000);

					// xor upper and lower
					lower.l20(0b0110);

					lower.updateSim();

					JmpCtrl out = in;
					out.jmp |= lower.O();
					return out;
				};
		}
#endif

#if 1
		auto aluExecute()
		{
			return [this](const AluOperands& op, const AluCtrl& ctrl)
				{
					BVec op1 = op.alu_operand[0] & ctrl.alu_enSumInputA | name("aluAdderOp1");
					BVec op2 = ConstBVec(op1.width());
					for (size_t i = 0; i < op2.size(); ++i)
					{
#if 1
						arch::colognechip::CC_MX4 mux;
						mux.D0() = ctrl.alu_truthTable[0];
						mux.D1() = ctrl.alu_truthTable[1];
						mux.D2() = ctrl.alu_truthTable[2];
						mux.D3() = ctrl.alu_truthTable[3];
						mux.S0() = op.alu_operand[0][i];
						mux.S1() = op.alu_operand[1][i];
						op2[i] = mux.Y();
#else
						op2[i] = mux(cat(op.alu_operand[1][i], op.alu_operand[0][i]), ctrl.alu_truthTable);
#endif
					}
					op2 = name(op2, "aluAdderOp2");

					std::array<arch::colognechip::CC_ADDF, 32> adder;
					std::array<Bit, 32> sum;
					for (size_t i = 0; i < adder.size(); ++i)
					{
						adder[i].A() = op1[i];
						adder[i].B() = op2[i];
						adder[i].CI() = i == 0 ? ctrl.alu_carryIn : adder[i - 1].CO();

						sum[i] = adder[i].S();
					}

					return AluResult{
						.alu_result = pack(sum),
						.alu_zero = adder.back().CO(),
						.alu_sign = adder.back().S(),
						.alu_carry = adder.back().CO(),
						.alu_in_sign = { op1.msb(), op2.msb() },
					};
				};
		}
#endif

#if 0
		auto readRegisterFile()
		{
			return [this](const RegCtrl& ctrl)
				{
					m_writeCtrl = constructFrom(ctrl);

					auto [r1, r2] = arch::colognechip::registerFile(
						ctrl.regReadAddr[0], ctrl.regReadAddr[1],
						m_writeCtrl.regWriteAddr, m_writeCtrl.regWrite, m_writeCtrl.regWriteValue
					);

					return RegOperands{ {
							r1,
							r2,
						} };
				};
		}

		template<StreamSignal T>
		void resultLoopBack(T&& result)
		{
			m_writeCtrl = get<RegCtrl>(result);
			m_writeCtrl.regWrite &= transfer(result);

			Base::resultLoopBack(move(result));
		}
#endif

	private:
		RegCtrl m_writeCtrl;
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::mod::GateMateBranchCtrl, carryInvert, enSigned, enCarry);
