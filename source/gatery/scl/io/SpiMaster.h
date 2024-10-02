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

#include "../stream/Stream.h"

namespace gtry::scl 
{
	class SpiMaster
	{
	public:
		SpiMaster& pin(std::string prefix) { return pin(prefix + "scl", prefix + "miso", prefix + "mosi"); }
		SpiMaster& pin(std::string clock, std::string miso, std::string mosi);
		SpiMaster& pinTestLoop();
		SpiMaster& clockDiv(UInt value);
		SpiMaster& outIdle(bool value);

		const Bit& scl() const { return m_clk; }
		const Bit& miso() const { return m_in; }
		const Bit& mosi() const { return m_out; }
		Bit& miso() { return m_in; }

		virtual RvStream<BVec> generate(RvStream<BVec>& in);

	private:
		Bit m_clk;
		Bit m_out;
		Bit m_in;
		UInt m_clockDiv;

		bool m_outIdle = false;
	};
}
