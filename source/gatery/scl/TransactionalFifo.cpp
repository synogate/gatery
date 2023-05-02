#include "gatery/pch.h"
#include "TransactionalFifo.h"

void gtry::scl::internal::generateCDCReqAck(const UInt& inData, UInt& outData, const Clock& inDataClock, const Clock& outDataClock)
{
	RvStream<UInt> inDataStream{ inData };
	valid(inDataStream) = '1';
	RvStream<UInt> synchronizableInDataStream = inDataStream.regDownstream(RegisterSettings{.clock = inDataClock});
	auto outDataStream = synchronizeStreamReqAck(synchronizableInDataStream, inDataClock, outDataClock);
	ENIF(valid(outDataStream))
		outData = reg(*outDataStream, 0 , RegisterSettings{ .clock = outDataClock });
	ready(outDataStream) = '1';
}
