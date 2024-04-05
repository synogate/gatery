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
#include "gatery/scl_pch.h"
#include "TransactionalFifo.h"
#include "stream/utils.h"
#include "stream/Stream.h"

void gtry::scl::internal::generateCDCReqAck(const UInt& inData, UInt& outData, const Clock& inDataClock, const Clock& outDataClock)
{
	RvStream<UInt> inDataStream{ inData };
	valid(inDataStream) = '1';
	RvStream<UInt> synchronizableInDataStream = strm::regDownstream(move(inDataStream), RegisterSettings{.clock = inDataClock});
	auto outDataStream = synchronizeStreamReqAck(synchronizableInDataStream, inDataClock, outDataClock);
	ENIF(valid(outDataStream))
		outData = reg(*outDataStream, 0 , RegisterSettings{ .clock = outDataClock });
	ready(outDataStream) = '1';
}
