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
#include "../arch/colognechip/RiscVopt.h"
#include "../arch/colognechip/Arithmetic.h"
#include "ModRegisterFile.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class GateMate : public Base
	{
	public:

	protected:

#if 1
		auto aluDiff() 
		{
			return [this](const OperandsDiff& op)
				{
					std::array<arch::colognechip::CC_ADDF, 32> adder;
					std::array<Bit, 32> sum;
					for (size_t i = 0; i < adder.size(); ++i)
					{
						adder[i].A() = op.dif_operand[0][i];
						adder[i].B() = ~op.dif_operand[1][i];
						adder[i].CI() = i == 0 ? Bit{ '1' } : adder[i - 1].CO();

						sum[i] = adder[i].S();
					}

					AluDiff ret;
					ret.diff = pack(sum);
					ret.diff_zero = ret.diff == 0;
					ret.diff_overflow = adder[30].CO() ^ adder[31].CO();
					ret.diff_sign = adder.back().S();
					ret.diff_carry = adder.back().CO();
					return ret;
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
