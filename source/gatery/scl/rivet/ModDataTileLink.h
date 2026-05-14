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

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class DataTileLink : public Base
	{
	public:
		DataTileLink()
		{
			Base::addInstr(InstrDef{ .name = "LOAD",  .opcode = "00000", .encoding = InstrEncoding::I, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .resultSource = "Mem" });
			Base::addInstr(InstrDef{ .name = "STORE", .opcode = "01000", .encoding = InstrEncoding::S, .sumSrc = {{OperandSource::Reg, OperandSource::Imm}}, .aluSrc = {{OperandSource::DontCare, OperandSource::Reg}} });
		}

		void dataMem(TileLinkU&& link)
		{
			m_link = constructFrom(link);
			link <<= move(m_link) | name("dataLink");
		}

		void waitForStoreCompletion(bool value) { m_waitForStore = value; }

	protected:

		auto dataMem()
		{
			return [this](StreamSignal auto&& in)
				{
					const MemCtrl& ctrl = get<MemCtrl>(in);
					Bit waitStart = (m_waitForStore ? transfer(m_link.a) : transfer(m_link.a) & ctrl.memRead) | name("memWaitStart");
					Bit responseValid = (m_waitForStore ? valid(*m_link.d) : valid(*m_link.d) & (*m_link.d)->hasData()) | name("memResponseValid");
					Bit readPending = flag(waitStart, responseValid & ready(*m_link.d)) | name("memAccessPending");

					Bit memAccess = (ctrl.memRead | ctrl.memWrite) | name("memAccess");
					valid(m_link.a) = valid(in) & memAccess & !readPending;
					m_link.a->opcode = cat(ctrl.memRead, '0', '0');
					m_link.a->param = 0;
					m_link.a->size = ctrl.memLogSize;
					m_link.a->source = 0;
					m_link.a->address = resizeTo(ctrl.memAddress, m_link.a->address.width());
					m_link.a->mask = ctrl.memWriteMask;
					m_link.a->data = ctrl.memWriteData;

					sim_assert(
						!valid(m_link.a) |
						!(m_link.a->address[0] & m_link.a->size > 0) &
						!(m_link.a->address[1] & m_link.a->size > 1)
					) << "invalid tile link access alignment";

					Bit waitCondition = m_waitForStore ? memAccess : ctrl.memRead;
					Stream ret = move(in) |
						Base::attach([this](const ResultCollection& in) { return in + ExecResult{ "Mem", (*m_link.d)->data }; }) |
						strm::stall(valid(in) & waitCondition & !responseValid | valid(m_link.a) & !ready(m_link.a));

					ready(*m_link.d) = ready(ret);
					return ret;
				};
		}

	private:
		TileLinkU m_link;

		bool m_waitForStore = false;
	};
}
