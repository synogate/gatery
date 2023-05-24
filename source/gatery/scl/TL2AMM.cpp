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
#include <gatery/scl/stream/StreamArbiter.h>
#include <gatery/scl/stream/adaptWidth.h>

struct TileLinkRequestSubset {
	BVec opcodeA = 3_b;
	UInt source;
};

BOOST_HANA_ADAPT_STRUCT(TileLinkRequestSubset, opcodeA, source);

TileLinkUL makeTlSlave(AvalonMM& avmm, BitWidth sourceW, size_t maxReadRequestsInFlight, size_t maxWriteRequestsInFlight)
{
	HCL_ASSERT_HINT(!avmm.response, "Avalon MM response not yet implemented");
	HCL_DESIGNCHECK_HINT(avmm.writeData, "These interfaces are not compatible. There is no writeData field in your AMM interface");
	auto ret = tileLinkInit<TileLinkUL>(avmm.address.width(), avmm.writeData->width(), sourceW);

	if (avmm.read)
		avmm.read = valid(ret.a) & ret.a->isGet();

	if (avmm.write)
		avmm.write = valid(ret.a) & ret.a->isPut();

	avmm.address = ret.a->address;
	avmm.writeData = (UInt)ret.a->data;

	if (avmm.byteEnable)
		avmm.byteEnable = (UInt)ret.a->mask;

	TileLinkD response = tileLinkDefaultResponse(*(ret.a));

	scl::Fifo<TileLinkD> writeRequestFifo{ maxWriteRequestsInFlight , response };
	HCL_NAMED(writeRequestFifo);
	scl::Fifo<TileLinkD> readRequestFifo{ maxReadRequestsInFlight , response };
	HCL_NAMED(readRequestFifo);

	if (avmm.ready)
		ready(ret.a) = avmm.ready.value();
	else
		ready(ret.a) = '1';

	ready(ret.a) &= !writeRequestFifo.full();
	ready(ret.a) &= !readRequestFifo.full();

	IF(transfer(ret.a)) {
		IF(ret.a->isGet()) {
			readRequestFifo.push(response);
		}
		ELSE{
			writeRequestFifo.push(response);
		}
	}

	scl::RvStream<TileLinkD> writeRes = { constructFrom(response) };
	writeRes <<= writeRequestFifo;
	scl::RvStream writeResBuffered = writeRes.regDownstream();

	scl::RvStream<TileLinkD> readRes = { constructFrom(response) };
	readRes <<= readRequestFifo;
	scl::RvStream readResBuffered = readRes.regDownstream();

	Bit responseReady = *avmm.read;
	if (!avmm.readDataValid)
		for (size_t i = 0; i < avmm.readLatency; ++i)
			responseReady = reg(responseReady, '0');
	else
		responseReady = *avmm.readDataValid;
	HCL_NAMED(responseReady);

	scl::RvStream readResStalled = stall(readResBuffered, !responseReady);

	HCL_DESIGNCHECK_HINT(avmm.readData, "These interfaces are not compatible. There is no readData field in your AMM interface");
	readResStalled->data = (BVec)*avmm.readData;
	HCL_NAMED(readResStalled);

	if (avmm.readDataValid)
		sim_assert(!*avmm.readDataValid | valid(readResStalled));
	else
		HCL_DESIGNCHECK(avmm.readLatency > 1);

	scl::StreamArbiter<scl::RvStream<TileLinkD>> responseArbiter;
	responseArbiter.attach(readResStalled, 0);
	responseArbiter.attach(writeResBuffered, 1);
	responseArbiter.generate();
	(*ret.d) <<= responseArbiter.out();

	readRequestFifo.generate();
	writeRequestFifo.generate();

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
