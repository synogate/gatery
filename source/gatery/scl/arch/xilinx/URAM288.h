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

#include <gatery/frontend/ExternalModule.h>

namespace gtry::scl::arch::xilinx
{
	class URAM288 : public ExternalModule
	{
	public:
		struct PortIn
		{
			BVec din = 72_b;
			UInt addr = 23_b;
			Bit en;
			Bit rdb_wr;
			BVec bwe = 9_b;
		};

		struct PortOut
		{
			BVec dout = 72_b;
			Bit sbiterr;
			Bit dbiterr;
			Bit rdaccess;
		};

		enum Port
		{
			A, B
		};

	public:
		URAM288();

		void clock(const Clock &clock);

		PortOut port(Port portId);
		void port(Port portId, const PortIn &portIn);

		void cascade(URAM288& in, size_t numRamsInTotal);
		void cascadeReg(bool enableCascadingReg);

		void enableOutputRegister(Port portId, bool enable);

	private:
		size_t m_cascadeAddress = 0;
	};

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::arch::xilinx::URAM288::PortIn,
	din,
	addr,
	en,
	rdb_wr,
	bwe
);

BOOST_HANA_ADAPT_STRUCT(gtry::scl::arch::xilinx::URAM288::PortOut,
	dout,
	sbiterr,
	dbiterr,
	rdaccess
);
