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
#include "../Adder.h"

namespace gtry::scl::rivet
{
	struct SumOperands
	{
		std::array<BVec, 2> sum_operand;
	};

	struct AluOperands
	{
		std::array<BVec, 2> alu_operand;
	};

	struct SumResult
	{
		BVec sum_result;
	};

	struct AluResult
	{
		// valid in mode add, sub, and, or, xor
		BVec alu_result;

		// valid in mode equal, less
		Bit alu_zero;
		Bit alu_overflow;
		Bit alu_sign;
		Bit alu_carry;
	};
}

namespace gtry::scl::rivet::mod
{
	struct AluCtrl
	{
		BVec alu_truthTable = 4_b;
		Bit alu_carryIn;
		Bit alu_enSumInputA;
	};

	inline AluResult alu(const AluCtrl& ctrl, const BVec& a, const BVec& b)
	{
		BVec op1 = a & ctrl.alu_enSumInputA | name("aluAdderOp1");
		BVec op2 = ConstBVec(op1.width());
		for (size_t i = 0; i < op2.size(); ++i)
			op2[i] = mux(cat(b[i], a[i]), ctrl.alu_truthTable);
		op2 = name(op2, "aluAdderOp2");

		auto [sum, carry] = add((UInt)op1, (UInt)op2, ctrl.alu_carryIn);

		return AluResult{
			.alu_result = sum,
			.alu_zero = carry.msb(),
			.alu_overflow = carry[carry.size() - 2] ^ carry.msb(),
			.alu_sign = sum.msb(),
			.alu_carry = carry.msb(),
		};
	}

	template<typename Base>
	class IntAlu : public Base
	{
	protected:

		auto aluDecode() {
			return [this](const InstrOpcode& instr)
				{
					AluCtrl ctrl{ .alu_truthTable = ConstBVec(4_b) };

					auto solve = [&, this](std::initializer_list<AluMode> active) -> Bit {
						return Base::findInstrCondition(instr, Base::instrAction(active, { AluMode::DontCare }, &InstrDef::aluMode)).value_or('0');
					};

					Bit arithSubInv = solve({ AluMode::Arith }) & instr.instr[30] | name("aluTableArithInv");

					ctrl.alu_truthTable = cat(
						/*				Logic,						Identity,				Arithmetic,						Compare */
						/*11*/ solve({ AluMode::And, AluMode::Or,	AluMode::A, AluMode::B,	AluMode::Add, AluMode::Arith,	AluMode::Equal }),
						/*10*/ solve({ AluMode::Xor, AluMode::Or,	AluMode::B,				AluMode::Add, AluMode::Arith,	}),
						/*01*/ solve({ AluMode::Xor, AluMode::Or,	AluMode::A,				AluMode::Sub,					AluMode::Less }),
						/*00*/ solve({ AluMode::And,										AluMode::Sub,					AluMode::Equal, AluMode::Less })
					) | name("aluTablePreArith");
					ctrl.alu_truthTable ^= arithSubInv;

					ctrl.alu_carryIn = solve({ AluMode::Sub, AluMode::Less, AluMode::Equal }) | name("aluCarryInPreArith");
					ctrl.alu_carryIn |= arithSubInv;

					ctrl.alu_enSumInputA = solve({ AluMode::Add, AluMode::Sub, AluMode::Arith, AluMode::Less });

					return ctrl;
				};
		}

		auto aluExecute() {
			return [this](const AluOperands& op, const AluCtrl& ctrl)
				{
					return mod::alu(ctrl, op.alu_operand[0], op.alu_operand[1]);
				};
		}

		auto aluSum() {
			return [](const SumOperands& op)
				{
					return SumResult{ (UInt)op.sum_operand[0] + (UInt)op.sum_operand[1] };
				};
		}

	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::SumOperands, sum_operand);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::AluOperands, alu_operand);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::SumResult, sum_result);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::AluResult, alu_result, alu_zero, alu_overflow, alu_sign, alu_carry);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::mod::AluCtrl, alu_truthTable, alu_carryIn, alu_enSumInputA);
