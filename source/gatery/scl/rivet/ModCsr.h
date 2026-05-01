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
#include "ModInstrFetch.h"

namespace gtry::scl::rivet
{
	struct CsrCtrl
	{
		UInt csrAddr;
		BVec csrReadData;
	};
}

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class Csr : public Base
	{
		struct CsrReg
		{
			size_t addr;
			BVec value;
		};
	public:
		Csr()
		{
			// simplified decoding for read only csr implementation
			Base::addInstr(InstrDef{ .name = "CSRRRW",	.opcode = "11100", .func3 = "001", .sumSrc = {{OperandSource::Zero, OperandSource::CSR}}, .resultSource = "Sum"});
			Base::addInstr(InstrDef{ .name = "CSRRRS",	.opcode = "11100", .func3 = "010", .sumSrc = {{OperandSource::Zero, OperandSource::CSR}}, .resultSource = "Sum"});
			Base::addInstr(InstrDef{ .name = "CSRRCI",	.opcode = "11100", .func3 = "011", .sumSrc = {{OperandSource::Zero, OperandSource::CSR}}, .resultSource = "Sum"});
			Base::addInstr(InstrDef{ .name = "CSRRI",	.opcode = "11100", .func3 = "1--", .sumSrc = {{OperandSource::Zero, OperandSource::CSR}}, .resultSource = "Sum"});
		}

		void addCsrReadOnly(size_t addr, const BVec& value) { m_register.push_back({ addr, value }); }

		void addCsrCycle(BitWidth regW)
		{
			UInt cycles = regW;
			cycles = reg(cycles + 1, 0) | name("csrCycle");
			addCsrReadOnly(0xC00, cycles);
		}

		void addCsrInstructionsRetired(BitWidth regW)
		{
			UInt instructions = regW;
			instructions = reg(instructions + m_instructionRetired, 0) | name("csrRetired");
			addCsrReadOnly(0xC02, instructions);
		}

		void addCsrTime(BitWidth regW, std::chrono::nanoseconds duration)
		{
			Bit tickTimer = reg(scl::Counter{ duration }.isLast(), '0') | name("csrTimeTick");

			UInt timeReg = regW;
			timeReg = reg(timeReg + tickTimer, 0) | name("csrTime");
			addCsrReadOnly(0xC01, timeReg);
		}

		void addCsrMachineInformation(uint32_t vendorId, uint32_t architectureId, uint32_t implementationId, uint32_t hartId, uint32_t configPtr)
		{
			if (vendorId)
				addCsrReadOnly(0xF11, vendorId);
			if (architectureId)
				addCsrReadOnly(0xF12, architectureId);
			if (implementationId)
				addCsrReadOnly(0xF13, implementationId);
			if (hartId)
				addCsrReadOnly(0xF14, hartId);
			if (configPtr)
				addCsrReadOnly(0xF15, configPtr);
		}

	protected:

		auto addCsrCtrl()
		{
			return [this](const InstrOpcode& instr)
				{
					return CsrCtrl{
						.csrAddr = (UInt)instr.instr(20, 12_b),
						.csrReadData = ConstBVec(0, Base::dataPathW()),
					};
				};
		}
		
		auto selectCsrReadData()
		{
			return [this](const CsrCtrl& in)
				{
					// this is not fully acurate since we might kill the instruction later
					// but counting later would also miss count
					m_instructionRetired = EnableScope::getFullEnable() | name("instructionRetired");

					CsrCtrl out = in;
					for (const CsrReg& r : m_register)
					{
						IF(in.csrAddr == r.addr)
							out.csrReadData = r.value.width() >= 32_b ? r.value.lower(32_b) : zext(r.value);
						if (r.value.width() > 32_b)
							IF(in.csrAddr == (r.addr | 0x80))
								out.csrReadData = zext(r.value(32, r.value.width() - 32_b));
					}
					return out;
				};
		}

	private:
		std::vector<CsrReg> m_register;
		Bit m_instructionRetired;
	};
}
