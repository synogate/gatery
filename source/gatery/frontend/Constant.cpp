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
#include "Constant.h"

#include "BVec.h"
#include "UInt.h"
#include "SInt.h"

gtry::BVec gtry::ConstBVec(uint64_t value, BitWidth width, std::string_view name)
{
	return Const<BVec>(value, width, name);
}

gtry::BVec gtry::ConstBVec(BitWidth width, std::string_view name)
{
	return Const<BVec>(width, name);
}

gtry::UInt gtry::ConstUInt(uint64_t value, BitWidth width, std::string_view name)
{
	return Const<UInt>(value, width, name);
}

gtry::UInt gtry::ConstUInt(BitWidth width, std::string_view name)
{
	return Const<UInt>(width, name);
}

gtry::SInt gtry::ConstSInt(int64_t value, BitWidth width, std::string_view name)
{
	return Const<SInt>(value, width, name);
}

gtry::SInt gtry::ConstSInt(BitWidth width, std::string_view name)
{
	return Const<SInt>(width, name);
}
