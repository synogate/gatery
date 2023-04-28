#include "gatery/pch.h"
#include "TransactionalFifo.h"

void gtry::scl::internal::generateCDCReqAck(const UInt& inData, UInt& outData, Clock& inDataClock, Clock& outDataClock)

{
	//then use this function twice to implement generateCDC and hopefully break the include loops (add include streams to the start of this file if it's needed
	RvStream<UInt> inDataStream{ inData };
	valid(inDataStream) = '1';
	RvStream<UInt> syncronizableInDataStream = inDataStream.regDownstream();
	auto outDataStream = synchronizeStreamReqAck(syncronizableInDataStream, inDataClock, outDataClock);
	outData = *outDataStream;
	ready(outDataStream) = '1';
}
