/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

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
#include "../tilelink/tilelink.h"
namespace gtry::scl
{
	class MemoryTester
	{
	public:
		template<TileLinkSignal TLink>
		void generate(TLink& mem)
		{
			Area ent{ "MemoryTester", true };
			HCL_NAMED(mem);

			enum class State
			{
				write,
				read
			};
			Reg<Enum<State>> state{ State::write };
			state.setName("state");

			BVec availableSourceId = BitWidth{ mem.a->source.width().count() };
			availableSourceId = reg(availableSourceId, BVec{ availableSourceId.width().mask() });
			HCL_NAMED(availableSourceId);

			// transmit side
			IF(transfer(mem.a))
			{
				availableSourceId[mem.a->source] = '0';
				mem.a->address += mem.a->data.width().bytes();
				
				IF(state.current() == State::write)
				{
					IF(mem.a->address == 0)
						state = State::read;
				}
			}

			VStream<UInt> nextSourceId = priorityEncoder((UInt)availableSourceId);
			mem.a->source = reg(*nextSourceId, 0);
			valid(mem.a) = reg(valid(nextSourceId), '0');

			mem.a->opcode = (size_t)TileLinkA::PutFullData;
			mem.a->param = 0;
			mem.a->address = reg(mem.a->address, 0);
			mem.a->size = (UInt)utils::Log2C(mem.a->data.width().bytes());
			mem.a->mask = (BVec)oext(0);
			IF(state.current() == State::read)
				mem.a->opcode = (size_t)TileLinkA::Get;
			
			mem.a->data = 0;
			if (mem.a->data.width() > mem.a->address.width())
				mem.a->data.lower(mem.a->address.width()) = (BVec)mem.a->address;
			else
				mem.a->data = (BVec)mem.a->address.lower(mem.a->data.width());

			Memory<BVec> expectedData(mem.a->source.width().count(), mem.a->data.width());
			IF(transfer(mem.a))
				expectedData[mem.a->source] = mem.a->data;

			// receive side
			TileLinkChannelD& d = *mem.d;
			ready(d) = '1';

			Bit error = '0';
			IF(transfer(d))
			{
				availableSourceId[d->source] = '1';
				
				error = d->error;
				IF(d->opcode == (size_t)TileLinkD::AccessAckData)
				{
					IF(d->data != expectedData[d->source])
						error = '1';
				}
			}

			error = reg(error, '0', {.allowRetimingBackward = true});
			
			m_errorCount = 16_b;
			IF(error)
				m_errorCount += 1;
			m_errorCount = reg(m_errorCount, 0);

			HCL_NAMED(error);
			HCL_NAMED(m_errorCount);
			HCL_NAMED(mem);
		}

		UInt numErrors() const { return m_errorCount; }

	private:
		UInt m_errorCount;


	};



}
