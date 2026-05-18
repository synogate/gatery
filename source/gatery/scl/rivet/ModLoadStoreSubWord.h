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
#include "ModLoadStore.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class LoadStoreSubword : public Base
	{
	protected:

		auto selectMemCtrl()
		{
			return [this](const MemCtrl& in, const InstrOpcode& instr, const SumResult& sum, const AluOperands& diff)
				{
					MemCtrl ctrl = Base::selectMemCtrl()(in, instr, sum, diff);

					Area ent{ "ent_debug", true };

					auto wrByte = ctrl.memWriteData.parts(4);
					ctrl.memWriteData = cat(
						mux(ctrl.memLogSize, { wrByte[0], wrByte[1], wrByte[3], wrByte[3] }),
						mux(ctrl.memLogSize.msb(), { wrByte[0], wrByte[2] }),
						mux(ctrl.memLogSize, { wrByte[0], wrByte[1], wrByte[1], wrByte[1] }), // use mux4 to keep it single level and share inputs with byte 3
						wrByte[0]
					);

					const UInt& ofs = ctrl.memAddress.lower(2_b);
					ctrl.memWriteMask = cat(
						ofs == 3 | ofs == 2 & ctrl.memLogSize == 1,
						ofs == 2,
						ofs == 1 | ofs == 0 & ctrl.memLogSize == 1,
						ofs == 0
					) | ctrl.memLogSize.msb();

					return ctrl | name("LoadStoreSubword");
				};
		}

	};
}
