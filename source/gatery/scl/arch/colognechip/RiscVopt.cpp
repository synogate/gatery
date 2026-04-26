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
#include "gatery/scl_pch.h"
#include "RiscVopt.h"
#include "BRAM.h"

namespace gtry::scl::arch::colognechip
{
	std::tuple<BVec, BVec> registerFile(UInt ra1, UInt ra2, UInt rd, Bit write, BVec writeData)
	{
		Area ent{ "registerFile_gatemate", true };
		HCL_NAMED(ra1);
		HCL_NAMED(ra2);
		HCL_NAMED(rd);
		HCL_NAMED(write);
		HCL_NAMED(writeData);

		UInt ra[2] = { ra1, ra2 };
		BVec r[2];
		CC_BRAM_40K ram[2];
		for (size_t i = 0; i < 2; ++i)
		{
			ram[i].address(CC_BRAM::A, rd, '1');
			ram[i].write(CC_BRAM::A, write, writeData, ConstBVec(writeData.width().mask(), writeData.width()));

			ram[i].address(CC_BRAM::B, ra[i], '1');
			r[i] = ram[i].read(CC_BRAM::B).lower(writeData.width());
			ram[i].generic("B_CLK_INV") = true;

			IF(write & ra[i] == rd)
				r[i] = writeData;
			r[i] = reg(r[i]);
		}

		HCL_NAMED(r);
		return { r[0], r[1] };
	}
}
