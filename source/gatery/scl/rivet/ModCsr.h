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
			Base::addInstr(InstrDef{ .name = "CSRRRW",	.opcode = "11100", .func3 = "001", .aluSrc = {{OperandSource::CSR, OperandSource::DontCare}}, .aluMode = AluMode::A, .resultSource = "Alu"});
			Base::addInstr(InstrDef{ .name = "CSRRRS",	.opcode = "11100", .func3 = "010", .aluSrc = {{OperandSource::CSR, OperandSource::DontCare}}, .aluMode = AluMode::A, .resultSource = "Alu"});
			Base::addInstr(InstrDef{ .name = "CSRRCI",	.opcode = "11100", .func3 = "011", .aluSrc = {{OperandSource::CSR, OperandSource::DontCare}}, .aluMode = AluMode::A, .resultSource = "Alu"});
			Base::addInstr(InstrDef{ .name = "CSRRI",	.opcode = "11100", .func3 = "1--", .aluSrc = {{OperandSource::CSR, OperandSource::DontCare}}, .aluMode = AluMode::A, .resultSource = "Alu"});
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
			m_instructionRetired.emplace();

			UInt instructions = regW;
			instructions = reg(instructions + *m_instructionRetired, 0) | name("csrRetired");
			addCsrReadOnly(0xC02, instructions);
		}

		void addCsrJumpMissPredict(BitWidth regW)
		{
			m_jumpMissPredict.emplace();

			UInt ctr = regW;
			ctr = reg(ctr + *m_jumpMissPredict, 0) | name("csrJumpMissPredict");
			addCsrReadOnly(0xCC0, ctr);
		}

		void addCsrJumpToSelf(BitWidth regW)
		{
			m_jumpToSelf.emplace();

			UInt ctr = regW;
			ctr = reg(ctr + *m_jumpToSelf, 0) | name("csrJumpToSelf");
			addCsrReadOnly(0xCC1, ctr);
		}

		void addCsrDecodeValid(BitWidth regW)
		{
			m_decodeValid.emplace();

			UInt ctr = regW;
			ctr = reg(ctr + *m_decodeValid, 0) | name("csrDecodeValid");
			addCsrReadOnly(0xCC2, ctr);
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
					if (m_decodeValid)
						*m_decodeValid = EnableScope::getFullEnable();

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

		template<StreamSignal T>
		void resultLoopBack(T&& result)
		{
			const JmpCtrl& ctrl = get<JmpCtrl>(result);
			const InstrPtr& ip = get<InstrPtr>(result);

			if(m_instructionRetired)
			{
				Bit isJts = (ctrl.jmp & ctrl.jmpTarget == ip.instrPtr) | name("isJts");
				*m_instructionRetired = transfer(result) & !isJts | name("instructionRetired");
			}
			if (m_jumpMissPredict)
			{
				*m_jumpMissPredict = transfer(result) & ctrl.jmp | name("jumpMissPredict");
			}
			if (m_jumpToSelf)
			{
				*m_jumpToSelf = transfer(result) & (ctrl.jmp & ctrl.jmpTarget == ip.instrPtr) | name("jumpToSelf");
			}

			return Base::resultLoopBack(move(result));
		}


	private:
		std::vector<CsrReg> m_register;

		std::optional<Bit> m_instructionRetired;
		std::optional<Bit> m_decodeValid;
		std::optional<Bit> m_jumpMissPredict;
		std::optional<Bit> m_jumpToSelf;
	};
}
