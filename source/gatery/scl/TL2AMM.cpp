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

struct TileLinkRequestSubset {
	BVec opcodeA = 3_b;
	UInt source;
};

BOOST_HANA_ADAPT_STRUCT(TileLinkRequestSubset, opcodeA, source);

TileLinkUL makeTlSlave(AvalonMM& avmm, BitWidth sourceW, size_t maxReadRequestsInFlight, size_t maxWriteRequestsInFlight)
{
	TileLinkUL ret;

	ret.a->address = constructFrom(avmm.address);

	HCL_DESIGNCHECK_HINT(avmm.writeData.has_value(), "These interfaces are not compatible. There is no writeData field in your AMM interface");
	ret.a->data = constructFrom((BVec)avmm.writeData.value());
	ret.a->size = BitWidth::count((ret.a->data).width().bytes());


	if (avmm.byteEnable) {
		ret.a->mask = constructFrom((BVec)avmm.byteEnable.value());
		avmm.byteEnable = (UInt)ret.a->mask;
	}
	else
		ret.a->mask = BitWidth((ret.a->data).width().bytes());


	ret.a->source = sourceW;

	if (avmm.ready)
		ready(ret.a) = avmm.ready.value();
	else
		ready(ret.a) = '1';

	TileLinkD response = tileLinkDefaultResponse(*(ret.a));
	// make fifos of responses then arbiter those fifos


	scl::Fifo<TileLinkD> writeRequestFifo{ maxWriteRequestsInFlight , response };
	HCL_NAMED(writeRequestFifo);
	scl::Fifo<TileLinkD> readRequestFifo{ maxReadRequestsInFlight , response };
	HCL_NAMED(readRequestFifo);

	sim_assert(!writeRequestFifo.full());
	sim_assert(!readRequestFifo.full());

	IF(transfer(ret.a)) {
		IF(ret.a->isGet()) {
			readRequestFifo.push(response);
		}
		ELSE{
			writeRequestFifo.push(response);
		}
	}

	valid(*ret.d) = !writeRequestFifo.empty() | avmm.readDataValid.value();


	*(*ret.d) = writeRequestFifo.peek();
	IF(avmm.readDataValid.value()){
		*(*ret.d) = readRequestFifo.peek();
	}

	IF(transfer(*ret.d)) {
		IF(avmm.readDataValid.value())
			readRequestFifo.pop();
		ELSE IF(!writeRequestFifo.empty())
			writeRequestFifo.pop();
	}

	readRequestFifo.generate();
	writeRequestFifo.generate();

	HCL_DESIGNCHECK_HINT(avmm.readData.has_value(), "These interfaces are not compatible. There is no readData field in your AMM interface");
	(*ret.d)->data = (BVec)avmm.readData.value();

	if (avmm.response)
	{
		HCL_ASSERT_HINT(false, "Avalon MM response not yet implemented");
	}
	
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

	ret.ready = ready(tlmm.a);
	ret.readDataValid = valid(*tlmm.d);
	ret.readData = (UInt)(*tlmm.d)->data;

	Bit byteEnable;
	if (!ret.byteEnable)
		byteEnable = '0';
	else
		byteEnable = ~(*ret.byteEnable) != 0;

	IF(ret.write.value()) {
		IF(byteEnable)
			tlmm.a->setupPutPartial(ret.address, (BVec)ret.writeData.value(), (BVec)ret.byteEnable.value(), 0);
		ELSE
			tlmm.a->setupPut(ret.address, (BVec)ret.writeData.value(), 0);
	} ELSE	IF(ret.read.value())
				tlmm.a->setupGet(ret.address, 0);
			ELSE
				undefined(tlmm.a->opcode);

	ret.response = 0;
	tlmm.a->source = 0;

	return ret;
}
