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
#include "ModResultSelect.h"
#include "ModLoadStore.h"
#include "ModControlFlow.h"
#include "ModDataTileLink.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class DataTileLinkReschedule : public Base
	{
	public:
		DataTileLinkReschedule()
		{
			Base::addInstr(InstrDef{ .name = "LOAD",  .opcode = "00000", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "Extern", .jmpTarget = "InstrPtr" });
			Base::addInstr(InstrDef{ .name = "STORE", .opcode = "01000", .encoding = InstrEncoding::S, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .aluSrc = {{OperandSource::DontCare, OperandSource::Reg}}, .jmpTarget = "InstrPtr" });
		}

		void dataMem(TileLinkU&& link)
		{
			m_link = constructFrom(link);
			link <<= move(m_link) | name("dataLink");
		}

	protected:

		auto dataMem()
		{
			return [this](StreamSignal auto&& in)
				{
					const MemCtrl& ctrl = get<MemCtrl>(in);

					Bit readDataValid;
					Bit operationPending;
					operationPending = flag(transfer(m_link.a) & ctrl.memRead, transfer(in) & readDataValid) | name("memAccessPending");

					ready(*m_link.d) = '1';
					readDataValid = flag(transfer(*m_link.d) & (*m_link.d)->hasData(), transfer(in) & readDataValid) | name("readDataValid");

					valid(m_link.a) = valid(in) & (ctrl.memRead | ctrl.memWrite) & !operationPending;
					m_link.a->opcode = cat(ctrl.memRead, '0', '0');
					m_link.a->param = 0;
					m_link.a->size = ctrl.memLogSize;
					m_link.a->source = 0;
					m_link.a->address = resizeTo(ctrl.memAddress, m_link.a->address.width());
					m_link.a->mask = ctrl.memWriteMask;
					m_link.a->data = ctrl.memWriteData;

					// capture data required for read data shifting.
					Bit zeroExtend = capture(get<InstrOpcode>(in).instr[14], valid(in)) | name("zeroExtend");
					UInt wordOfs = capture(ctrl.memAddress.lower(2_b), valid(in)) | name("wordOfs");
					UInt logSize = capture(ctrl.memLogSize, valid(in)) | name("logSize");

					BVec readData = (*m_link.d)->data | name("readData");
					BVec shiftedReadData = selectReadDataFromWordAccess(readData, wordOfs, logSize, zeroExtend);

					IF(transfer(*m_link.d) & (*m_link.d)->hasData())
						Base::externalResult(shiftedReadData);

					Bit instrReady = (
						ctrl.memWrite & ready(m_link.a) |
						ctrl.memRead & readDataValid
						) | name("memAddressInstrReady");

					return move(in) |
						Base::attach([&](const JmpCtrl& jmp, const InstrPtr& ip, const MemCtrl& ctrl) {
							JmpCtrl out = jmp;
							out.jmp &= !instrReady;
							return out;
						}) |
						Base::attach([&](const RegCtrl& reg) {
							RegCtrl out = reg;
							out.regWrite &= instrReady | !ctrl.memRead;
							return out;
						});
				};
		}

	private:
		TileLinkU m_link;

		bool m_waitForStore = false;
	};
}
