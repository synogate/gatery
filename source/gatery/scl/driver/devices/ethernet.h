#pragma once
#include "device.h"
#include "stream.h"

struct EthDevice
{
	Stream<TransactionalDevice, StreamRxDevice<unsigned long>, PacketRxDevice> rx;
	Stream<TransactionalDevice, StreamTxDevice<unsigned long>, PacketTxDevice> tx;
};
