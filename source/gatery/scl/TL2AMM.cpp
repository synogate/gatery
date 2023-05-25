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

TileLinkUL makeTlSlave(AvalonMM& avmm, BitWidth sourceW, size_t maxReadRequestsInFlight, size_t maxWriteRequestsInFlight)
{
	HCL_ASSERT_HINT(!avmm.response, "Avalon MM response not yet implemented");
	HCL_DESIGNCHECK_HINT(avmm.writeData, "These interfaces are not compatible. There is no writeData field in your AMM interface");
	BitWidth excessBits = BitWidth::count(avmm.writeData->width().bytes());
	auto ret = tileLinkInit<TileLinkUL>(avmm.address.width() + excessBits, avmm.writeData->width(), sourceW);

	if (avmm.read)
		avmm.read = valid(ret.a) & ret.a->isGet();

	if (avmm.write)
		avmm.write = valid(ret.a) & ret.a->isPut();

	avmm.address = ret.a->address.upper(-excessBits);
	avmm.writeData = (UInt)ret.a->data;

	HCL_DESIGNCHECK_HINT(avmm.byteEnable.has_value() || (*avmm.writeData).width() == 8_b, "You must have a byteEnable field if you want to have the granularity of interacting with specific bytes");
	avmm.byteEnable = (UInt)ret.a->mask;


	TileLinkD response = tileLinkDefaultResponse(*(ret.a));
	response.data.resetNode();
	response.data = 0_b;

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

	HCL_DESIGNCHECK_HINT(avmm.readData, "These interfaces are not compatible. There is no readData field in your AMM interface");
	scl::RvStream<UInt> readData(*avmm.readData);
	valid(readData) = responseReady;

	scl::RvStream<UInt> readDataFifo = readData.fifo(true, maxReadRequestsInFlight);

	scl::RvStream readResStalled = stall(readResBuffered, !valid(readDataFifo));
	ready(readDataFifo) = ready(readResStalled);

	readResStalled->data = (BVec)*readDataFifo;
	HCL_NAMED(readResStalled);

	if (avmm.readDataValid)
		sim_assert(!valid(readDataFifo) | valid(readResStalled));
	else
		HCL_DESIGNCHECK(avmm.readLatency > 1);

	writeResBuffered->data = ConstBVec(readResStalled->data.width());

	scl::StreamArbiter<scl::RvStream<TileLinkD>> responseArbiter;
	responseArbiter.attach(readResStalled, 0);
	responseArbiter.attach(writeResBuffered, 1);
	responseArbiter.generate();
	(*ret.d) <<= responseArbiter.out();

	readRequestFifo.generate();
	writeRequestFifo.generate();

	return ret;
}

AvalonMM makeAmmSlave(TileLinkUL& tlmm)
{
	HCL_DESIGNCHECK_HINT(false, "not yet implemented");

	return AvalonMM();
}
