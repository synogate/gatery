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
#include "ModInstrBoolSolver.h"
#include "ModControlFlow.h"
#include "ModIntAlu.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class Branch : public Base
	{
	protected:

		auto makeBranchDecision()
		{
			return [this](const JmpCtrl& in, const AluDiff& flags, const InstrOpcode& instr)
				{
					BVec func3 = instr.instr(12, 3_b);

					Bit decision = mux(func3(1, 2_b), {
						flags.diff_zero,						// equal
						Bit{'X'},								// unused
						flags.diff_sign != flags.diff_overflow,	// less than
						!flags.diff_carry						// less than unsigned
					}) ^ func3.lsb() | name("branch_decision");

					std::vector<MatchValue> actions;
					actions.reserve(Base::instructions().size());
					for (const InstrDef& def : Base::instructions())
						if (def.name == "BRA")
							actions.push_back(MatchValue::Low);
						else if (def.jmpTarget.empty())
							actions.push_back(MatchValue::DontCare);
						else
							actions.push_back(MatchValue::High);

					Bit notBranch = Base::findInstrCondition(instr, actions).value_or('1') | name("unconditionalJump");;

					JmpCtrl ctrl = in;
					ctrl.jmp &= notBranch | decision;
					return ctrl;
				};
		}

	};
}
