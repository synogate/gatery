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

namespace gtry::scl::rivet
{
	struct RegOperands
	{
		std::array<BVec, 2> regOperand;
	};

	struct RegCtrl
	{
		Bit regWrite;
		UInt regWriteAddr;
		BVec regWriteValue;

		std::array<Bit, 2> regRead;
		std::array<UInt, 2> regReadAddr;
	};
}

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class RegisterFile : public Base
	{
	public:

	protected:

		auto addRegCtrl() {
			return [this](const InstrOpcode& instr)
				{
					RegCtrl ctrl{
						.regWriteAddr = (UInt)instr.instr(7, 5_b),
						.regWriteValue = ConstBVec(Base::dataPathW()),
						.regReadAddr = {
							(UInt)instr.instr(15, 5_b),
							(UInt)instr.instr(20, 5_b),
						},
					};

					ctrl.regRead[0] = Base::findInstrCondition(instr,
						Base::instrAction({ InstrEncoding::R, InstrEncoding::I, InstrEncoding::S, InstrEncoding::SB }, { InstrEncoding::DontCare }, &InstrDef::encoding)
					).value_or('0') | name("is_regRead1_instr");

					ctrl.regRead[1] = Base::findInstrCondition(instr, 
						Base::instrAction({ InstrEncoding::R, InstrEncoding::S, InstrEncoding::SB }, { InstrEncoding::DontCare }, &InstrDef::encoding)
					).value_or('0') | name("is_regRead2_instr");

					std::vector<MatchValue> actions;
					actions.reserve(Base::instructions().size());
					for (const InstrDef& def : Base::instructions())
						actions.push_back(def.resultSource.empty() ? MatchValue::Low : MatchValue::High);
					ctrl.regWrite = Base::findInstrCondition(instr, actions).value_or('0') | name("is_regWrite_instr");
					ctrl.regWrite &= ctrl.regWriteAddr != 0;
					return ctrl;
				};
		}

		auto selectRegWriteValue()
		{
			return [this](const ResultCollection& results, const InstrOpcode& instr, const RegCtrl& in)
				{
					RegCtrl out = in;
					out.regWriteValue = Base::selectResult("regWrite", results, instr, &InstrDef::resultSource);
					return out;
				};
		}

		auto readRegisterFile()
		{
			return [this](const InstrOpcode& inst, const RegCtrl& ctrl)
				{
					{
						EnableScope s{ EnableScope::Always{} };
						m_registerFileMem.setup(ctrl.regWriteAddr.width().count(), Base::dataPathW());
						m_registerFileMem.setName("registerFile");
						m_registerFileMem.initZero();

						m_result = constructFrom(ctrl);
						IF(m_result.regWrite)
							m_registerFileMem[m_result.regWriteAddr] = m_result.regWriteValue;
					}

					return RegOperands{ {
							m_registerFileMem[ctrl.regReadAddr[0]].read(),
							m_registerFileMem[ctrl.regReadAddr[1]].read()
						} };
				};
		}

		template<StreamSignal T>
		void resultLoopBack(T&& result)
		{
			const RegCtrl& ctrl = get<RegCtrl>(result);
			m_result = get<RegCtrl>(result);
			m_result.regWrite &= transfer(result);

			Base::resultLoopBack(move(result));
		}


	private:
		Memory<BVec> m_registerFileMem;
		RegCtrl m_result;
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::RegOperands, regOperand);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::RegCtrl, regWrite, regWriteAddr, regWriteValue, regRead, regReadAddr);
