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
#include <gatery/frontend/Bit.h>
#include <gatery/frontend/ExternalModule.h>

namespace gtry::scl::arch::colognechip
{
	class CC_DFF : public ExternalModule
	{
	public:
		CC_DFF();

		Bit& D() { return in("D"); }
		Bit& EN() { return in("EN"); }
		Bit& SR() { return in("SR"); }
		Bit Q() { return out("Q"); }

		CC_DFF& clkInv(bool invert) { generic("CLK_INV") = invert ? 1 : 0; return *this; }
		CC_DFF& enInv(bool invert) { generic("EN_INV") = invert ? 1 : 0; return *this; }
		CC_DFF& srInv(bool invert) { generic("SR_INV") = invert ? 1 : 0; return *this; }
		CC_DFF& srVal(bool value) { generic("SR_VAL") = value ? 1 : 0; return *this; }
		CC_DFF& init(bool value) { generic("INIT") = value ? 1 : 0; return *this; }
	};


	class CC_DLT : public ExternalModule
	{
	public:
		CC_DLT();

		Bit& D() { return in("D"); }
		Bit& G() { return in("G"); }
		Bit& SR() { return in("SR"); }
		Bit Q() { return out("Q"); }

		CC_DLT& gInv(bool invert) { generic("G_INV") = invert ? 1 : 0; return *this; }
		CC_DLT& srInv(bool invert) { generic("SR_INV") = invert ? 1 : 0; return *this; }
		CC_DLT& srVal(bool value) { generic("SR_VAL") = value ? 1 : 0; return *this; }
		CC_DLT& init(bool value) { generic("INIT") = value ? 1 : 0; return *this; }
	};

}

