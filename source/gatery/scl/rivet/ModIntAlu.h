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
	struct OperandsSum
	{
		std::array<BVec, 2> sum_operand;
	};

	struct OperandsDiff
	{
		std::array<BVec, 2> dif_operand;
	};

	struct AluSum
	{
		BVec sum;
	};

	struct AluDiff
	{
		BVec diff;
		Bit diff_zero;
		Bit diff_overflow;
		Bit diff_sign;
		Bit diff_carry;
	};
}

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class IntAlu : public Base
	{
	protected:

		auto aluDiff() {
			return [this](const OperandsDiff& op)
				{
					auto [sum, carry] = add((UInt)op.dif_operand[0], (UInt)~op.dif_operand[1], '1');

					return AluDiff{
						.diff = sum,
						.diff_zero = sum == 0,
						.diff_overflow = carry[carry.size() - 2] ^ carry.msb(),
						.diff_sign = sum.msb(),
						.diff_carry = carry.msb(),
					};
				};
		}

		auto aluSum() {
			return [](const OperandsSum& op)
				{
					return AluSum{ (UInt)op.sum_operand[0] + (UInt)op.sum_operand[1] };
				};
		}

	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::OperandsSum, sum_operand);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::OperandsDiff, dif_operand);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::AluSum, sum);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::AluDiff, diff, diff_zero, diff_overflow, diff_sign, diff_carry);
