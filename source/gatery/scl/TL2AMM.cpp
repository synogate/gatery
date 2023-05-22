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
#include "TL2AMM.h"

TileLinkUL translators::makeTlSlave(AvalonMM amm, BitWidth sourceW)
{
	TileLinkUL ret;

	ret.a->address = amm.address;

	HCL_DESIGNCHECK_HINT(amm.writeData.has_value(), "These interfaces are not compatible. There is no writeData field in your AMM interface");
	ret.a->data = (BVec) amm.writeData.value();
	ret.a->size = BitWidth::count((ret.a->data).width().bytes());


	if (amm.byteEnable)
		ret.a->mask = (BVec)amm.byteEnable.value();
	else {
		ret.a->mask = (ret.a->data).width().bytes();
		ret.a->mask = (ret.a->mask).width().mask();
	}
	ret.a->source = sourceW;

	HCL_DESIGNCHECK_HINT(amm.readData.has_value(), "These interfaces are not compatible. There is no readData field in your AMM interface");
	(*ret.d)->data = (BVec) amm.readData.value();
	(*ret.d)->size = BitWidth::count(((*ret.d)->data).width().bytes());

	(*ret.d)->source = sourceW;
	if (amm.response)
		{HCL_ASSERT_HINT(false, "avalon MM response not yet implemented");}
	else 
		(*ret.d)->error = '0';

	if (amm.ready)	
		ready(ret.a) = amm.ready.value();
	else			
		ready(ret.a) = '1';

	valid(*ret.d) = flag(valid(ret.a) & ret.a->isPut(), ready(*ret.d)) | amm.readDataValid.value();

	UInt source = constructFrom((*ret.d)->source);
	IF(transfer(ret.a))
		source = ret.a->source;
	source = reg(source, 0);

	(*ret.d)->source = undefined(source);
	IF(valid(*ret.d))
		(*ret.d)->source = source;
	
	return ret;
}

AvalonMM translators::makeAmmSlave(TileLinkUL tlmm)
{
	return AvalonMM();
}
