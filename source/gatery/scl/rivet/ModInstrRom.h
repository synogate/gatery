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

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class InstrRom : public Base
	{
	public:
		void instrRom(std::vector<std::byte> data) { m_instrRomContent.swap(data); }
		void instrRom(std::span<const std::byte> data) { m_instrRomContent.clear(); m_instrRomContent.insert(m_instrRomContent.begin(), data.begin(), data.end()); }

	protected:

		auto instrFetch()
		{
			return [this](StreamSignal auto&& in)
				{
					return move(in) | 
						Base::attach([this](const InstrPtr& ptr) { return generateInstrFetch(ptr); }) |
						Base::pipelineStage("fetch");
				};
		}

	private:

		InstrOpcode generateInstrFetch(const InstrPtr& ptr)
		{
			Memory<UInt> imem(m_instrRomContent.size() + 3 / 4, 32_b);
			imem.fillPowerOnState(sim::createDefaultBitVectorState(m_instrRomContent));

			return { imem[ptr.instrPtr.upper(-2_b)].read() };
		}

		std::vector<std::byte> m_instrRomContent;
	};
}
