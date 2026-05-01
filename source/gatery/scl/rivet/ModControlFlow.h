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
#include "ModInstrBoolSolver.h"
#include "ModInstrSet.h"

namespace gtry::scl::rivet
{
	struct InstrOpcode;
	struct ResultCollection;

	struct InstrPtr
	{
		UInt instrPtr;
	};

	struct InstrSize
	{
		UInt instrLogSize;
	};

	struct JmpCtrl
	{
		Bit jmp;
		UInt jmpTarget;
	};
}

namespace gtry::scl::rivet::mod
{
	/**
	 * @brief Provides the default implementation for the instruction pointer.
	 *			- auto increments by 4 bytes
	 *			- combinatorial jump
	 */
	template<typename T>
	class ControlFlow : public T
	{
	public:
		void instrStartAddress(size_t addr) { m_startAddress = addr; }

	protected:

		auto addInstrPtr() {
			return [this]() {
				m_increment = EnableScope::getFullEnable();
				return InstrPtr{ m_instrPtr };
			};
		}

		auto addInstrSize() {
			return []() {
				return InstrSize{ 2 };
			};
		}

		auto addJmpCtrl() {
			return [this](const InstrOpcode& instr)
			{
				std::vector<MatchValue> actions;
				for (const InstrDef& def : T::instructions())
					actions.push_back(def.jmpTarget.empty() ? MatchValue::Low : MatchValue::High);

				return JmpCtrl{
					.jmp = T::findInstrCondition(instr, actions).value_or('0'),
					.jmpTarget = ConstUInt(m_instrPtr.width()),
				};
			};
		}

		auto selectJmpTarget()
		{
			return [this](const ResultCollection& results, const InstrOpcode& instr, const JmpCtrl& in)
				{
					return JmpCtrl{
						.jmp = in.jmp,
						.jmpTarget = T::selectResult("jump_target", results, instr, &InstrDef::jmpTarget),
					};
				};
		}

		void resultLoopBack(StreamSignal auto&& result)
		{
			UInt instrPtrReg = reg(m_instrPtr.width(), m_startAddress, "instrPtr");

			const JmpCtrl& ctrl = get<JmpCtrl>(result);
			IF(transfer(result) & ctrl.jmp)
				instrPtrReg = ctrl.jmpTarget;
			m_instrPtr = instrPtrReg;

			instrPtrReg += zext(4 & m_increment);

			T::resultLoopBack(move(result));
		}

	private:
		UInt m_instrPtr = 32_b;
		Bit m_increment;
		size_t m_startAddress = 0;
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::InstrPtr, instrPtr);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::JmpCtrl, jmp, jmpTarget);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::InstrSize, instrLogSize);
