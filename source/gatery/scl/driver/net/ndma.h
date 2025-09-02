#pragma once
#include <stdint.h>
#include "udp.h"

namespace udp { struct PacketInfo; }

enum class NdmaCommand : uint16_t
{
	put,		// put param1 bytes to memory address param2. devices usualy require alignments of address and size. doubles as stdout on the host system.
	clear,		// clear param1 bytes at memory address param2.
	call,		// call address param2
	ack,		// ack command in param1 and param2 returned in param2
};

struct NdmaHeader
{
	NdmaCommand cmd;
	uint16_t param1;
	uint32_t param2;
};

void ndmaProcessPacket(EthDevice& eth);
void ndmaSendMsg(EthDevice& eth, udp::PacketInfo& pkt, const char* msg, uint32_t len);
