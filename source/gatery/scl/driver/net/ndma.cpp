#include "ndma.h"
#include <array>
#include <algorithm>

void ndmaSendMsg(EthDevice& eth, udp::PacketInfo& pkt, const char* msg, uint32_t len)
{
	std::array<uint32_t, 512> buffer;
	buffer[0] = len << 16;
	buffer[1] = 0;
	size_t cnt = std::min<uint32_t>(len, 500);
	for (char* it = (char*)&buffer[2]; cnt; cnt--)
		*it++ = *msg++;
	udp::sendUdpPacket(eth, pkt, std::span(buffer.data(), (len + 3 + 8) / 4));
}

void ndmaProcessPacket(EthDevice& eth)
{
	if (!eth.rx.valid())
		return;

	udp::PacketInfo pkt;

	if (parseUdpPacket(eth, pkt) == 23232)
	{
		std::array<uint32_t, 2> param = {
			udp::parsePop(eth),
			udp::parsePop(eth),
		};
		NdmaCommand cmd = (NdmaCommand)(param[0] & 0xFFFF);
		param[0] >>= 16;
		uint32_t* dst = (uint32_t*)(uintptr_t)param[1];

		// we do put before crc check for performance reasons. we should move crc to hw...
		if (cmd == NdmaCommand::put)
			for (uint32_t* it = dst; it != dst + param[0] / 4 && !eth.rx.eop(); ++it)
				*it = udp::parsePop(eth);

		while (!eth.rx.eop())
			udp::parsePop(eth);

		if (cmd == NdmaCommand::clear)
			for (uint32_t* it = dst; it != dst + param[0] / 4; ++it)
				*it = 0;

		param[0] = uint32_t(NdmaCommand::ack) | uint32_t(cmd) << 16;
		udp::sendUdpPacket(eth, pkt, param);

		// we execute call after sending ack. we might never come back.
		if (cmd == NdmaCommand::call)
		{
			eth.rx.pop();
			eth.rx.commit();
			(*((void (*)(void))((uintptr_t)param[1])))();
			return;
		}
	}
	while (!eth.rx.eop())
		eth.rx.pop();
	eth.rx.pop();
	eth.rx.commit();
}
