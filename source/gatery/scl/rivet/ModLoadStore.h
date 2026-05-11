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
#include "ModIntAlu.h"

namespace gtry::scl::rivet
{
	struct MemCtrl
	{
		Bit memRead;
		Bit memWrite;
		UInt memAddress;
		UInt memLogSize;
		BVec memWriteData;
		BVec memWriteMask;
	};

}

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class LoadStore : public Base
	{
	public:
		LoadStore()
		{
			Base::addInstr(InstrDef{ .name = "LOAD",  .opcode = "00000", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "Mem" });
			Base::addInstr(InstrDef{ .name = "STORE", .opcode = "01000", .encoding = InstrEncoding::S, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .aluSrc = {{OperandSource::DontCare, OperandSource::Reg}} });
		}

	protected:

		auto addMemCtrl()
		{
			return [this](const InstrOpcode& instr)
				{
					using namespace std::literals;

					MemCtrl ctrl;
					ctrl.memWrite = Base::findInstrCondition(instr, Base::instrAction({ "STORE"sv }, {}, &InstrDef::name)).value_or('0');
					ctrl.memRead = Base::findInstrCondition(instr, Base::instrAction({ "Mem"sv }, {}, &InstrDef::resultSource)).value_or('0');
					ctrl.memAddress = ConstUInt(32_b);
					ctrl.memLogSize = (UInt)instr.instr(12, 2_b);
					ctrl.memWriteData = ConstBVec(Base::dataPathW());
					ctrl.memWriteMask = ConstBVec(Base::dataPathW() / 8);
					return ctrl;
				};
		}

		auto selectMemCtrl()
		{
			return [this](const MemCtrl& in, const InstrOpcode& instr, const SumResult& sum, const AluOperands& diff)
				{
					MemCtrl ctrl = in;
					ctrl.memAddress = (UInt)sum.sum_result;
					ctrl.memWriteData = diff.alu_operand[1];
					ctrl.memWriteMask = cat(ctrl.memLogSize[1], ctrl.memLogSize[1], ctrl.memLogSize != 0, '1');
					return ctrl;
				};
		}

		auto shiftMemReadData()
		{
			return [this](const MemCtrl& ctrl)
				{
					BitWidth wordW = ctrl.memWriteData.width();
					BitWidth subAddrW = BitWidth::count(wordW.bytes());
					sim_assert(ctrl.memRead & ctrl.memAddress.lower(subAddrW) == 0) << "unaligned memory read access fail";
					return ctrl;
				};
		}

	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::MemCtrl, memRead, memWrite, memAddress, memLogSize, memWriteData, memWriteMask);
