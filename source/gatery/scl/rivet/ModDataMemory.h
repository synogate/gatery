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
	class DataMemory : public Base
	{
	public:
		void dataMemSize(size_t size) { m_dataMemSize = size; }
		void dataMemContent(std::vector<std::byte> data) { m_dataMemContent.swap(data); }
		void dataMemContent(std::span<const std::byte> data) { m_dataMemContent.clear(); m_dataMemContent.insert(m_dataMemContent.begin(), data.begin(), data.end()); }

	protected:

		auto dataMem()
		{
			return [this](StreamSignal auto&& in)
				{
					MemCtrl& ctrl = get<MemCtrl>(in);
					IF(valid(in) & (ctrl.memRead | ctrl.memWrite))
					{
						sim_assert(ctrl.memLogSize == 2) << "byte and half memory access not implemented\n";
						sim_assert(ctrl.memAddress.lower(2_b) == 0) << "unaligned memory access not supported\n";
					}
		
					Bit shouldStall = valid(in) & ctrl.memRead;
					Bit secondCycle;
					secondCycle = flag(valid(in) & shouldStall, secondCycle) | name("memSecondCycle");

					size_t numWords = std::max(m_dataMemSize, m_dataMemContent.size()) + 3 / 4;
					Memory<BVec> dmem(numWords, 32_b);
					dmem.setName("dmem");
					dmem.fillPowerOnState(sim::createDefaultBitVectorState(m_dataMemContent));
					dmem.setType(MemType::DONT_CARE, 1);

					BVec readData = reg(
						dmem[ctrl.memAddress.upper(-2_b)].read(),
						{ .allowRetimingBackward = true }
					) | name("memReadData");

					get<ResultCollection>(in).set({ "Mem", readData });

					IF(valid(in) & ctrl.memWrite)
						dmem[ctrl.memAddress] = ctrl.memWriteData;

					return strm::stall(move(in), shouldStall & !secondCycle);
				};
		}

	private:

		

		size_t m_dataMemSize = 0;
		std::vector<std::byte> m_dataMemContent;
	};
}
