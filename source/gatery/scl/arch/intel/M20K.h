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

#include "../general/GenericMemory.h"

#include "IntelBlockram.h"

namespace gtry::scl::arch::intel {

class IntelDevice;

class M20K : public IntelBlockram 
{
	public:
		M20K(const IntelDevice &intelDevice);
};

class M20KStratix10Agilex : public M20K 
{
	public:
		M20KStratix10Agilex(const IntelDevice &intelDevice);
};


}
