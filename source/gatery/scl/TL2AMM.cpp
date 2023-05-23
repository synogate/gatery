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

TileLinkUL makeTlSlave(AvalonMM& avmm, BitWidth sourceW)
{
	TileLinkUL ret;

	ret.a->address = constructFrom(avmm.address);

	HCL_DESIGNCHECK_HINT(avmm.writeData.has_value(), "These interfaces are not compatible. There is no writeData field in your AMM interface");
	ret.a->data = constructFrom((BVec)avmm.writeData.value());
	ret.a->size = BitWidth::count((ret.a->data).width().bytes());


	if (avmm.byteEnable) {
		ret.a->mask = constructFrom((BVec)avmm.byteEnable.value());
		avmm.byteEnable = (UInt) ret.a->mask;
	}
	else 
		ret.a->mask = BitWidth((ret.a->data).width().bytes());


	ret.a->source = sourceW;

	if (avmm.ready)	
		ready(ret.a) = avmm.ready.value();
	else			
		ready(ret.a) = '1';

	HCL_DESIGNCHECK_HINT(avmm.readData.has_value(), "These interfaces are not compatible. There is no readData field in your AMM interface");
	(*ret.d)->data = (BVec)avmm.readData.value();
	(*ret.d)->size = BitWidth::count(((*ret.d)->data).width().bytes());

	(*ret.d)->source = sourceW;
	(*ret.d)->sink = 0_b;
	if (avmm.response)
		{HCL_ASSERT_HINT(false, "Avalon MM response not yet implemented");}
	else 
		(*ret.d)->error = '0';


	valid(*ret.d) = flag(valid(ret.a) & ret.a->isPut(), transfer(*ret.d)) | avmm.readDataValid.value();
	
	UInt source = constructFrom((*ret.d)->source);
	IF(transfer(ret.a))
		source = ret.a->source;
	source = reg(source, 0);

	(*ret.d)->source = undefined(source);
	IF(valid(*ret.d))
		(*ret.d)->source = source;
	

	BVec opcode = constructFrom((*ret.d)->opcode);
	IF(transfer(ret.a)) {
		IF(ret.a->isGet()) {
			opcode = (size_t) TileLinkD::AccessAckData;
		}
		ELSE{
			opcode = (size_t) TileLinkD::AccessAck;
		}
	}
	
	opcode = reg(opcode, BVec("3b"));

	(*ret.d)->opcode = undefined( opcode);
	IF(valid(*ret.d))
		(*ret.d)->opcode = opcode;
	


	
	
	if (avmm.read.has_value())
		avmm.read = valid(ret.a) & ret.a->isGet();
	else
		HCL_ASSERT_HINT(false, "Your Avalon Slave does not have a read signal, making it incompatible with TL slaves");

	if (avmm.write.has_value())
		avmm.write = valid(ret.a) & ret.a->isPut();
	else
		HCL_ASSERT_HINT(false, "Your Avalon Slave does not have a write signal, making it incompatible with TL slaves");


	avmm.address = ret.a->address;
	avmm.writeData = (UInt) ret.a->data;


	return ret;
}

AvalonMM makeAmmSlave(TileLinkUL tlmm)
{
	AvalonMM ret;

	ret.address = tlmm.a->address;
	ret.ready = ready(tlmm.a);
	ret.readDataValid = valid(*tlmm.d);
	ret.byteEnable = (UInt) tlmm.a->mask;

	
	ret.write = tlmm.a->isPut();
	ret.writeData = (UInt)tlmm.a->data;

	ret.read = tlmm.a->isGet();
	ret.readData = (UInt) (*tlmm.d)->data;

	ret.response = 0;
	tlmm.a->source = 0;

	return AvalonMM();
}
