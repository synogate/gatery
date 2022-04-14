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
#include "gatery/pch.h"
#include "UInt.h"

namespace gtry {


	UInt ext(const Bit& bit, size_t increment, Expansion policy)
	{
		SignalReadPort port = bit.readPort();
		port.expansionPolicy = policy;
		if (increment)
			port = port.expand(1 + increment, hlim::ConnectionType::BITVEC);
		return UInt(port);
	}

	UInt ext(const UInt& bvec, size_t increment, Expansion policy)
	{
		SignalReadPort port = bvec.readPort();
		port.expansionPolicy = policy;
		if (increment)
			port = port.expand(bvec.size() + increment, hlim::ConnectionType::BITVEC);
		return UInt(port);
	}
	
}
