#include "udp.h"

void udp::packetLoopback(EthDevice* dev)
{
	EthCrc32 crc;

	bool eop;
	do
	{
		while (!dev->rx.valid());
		unsigned data = dev->rx.payload();
		eop = dev->rx.eop();
		dev->rx.pop_unsafe();

		dev->tx.eop(eop);
		dev->tx.push(data);
	} while (!eop);

	dev->tx.commit();
	dev->rx.commit();
}

uint16_t udp::checksumIp(std::initializer_list<uint32_t> list)
{
	uint32_t checksum = 0;
	for (uint32_t v : list)
	{
		checksum += v & 0xFFFF;
		checksum += v >> 16;
	}
	checksum = checksum + (checksum >> 16);
	return ~checksum;
}

uint16_t udp::parseEthHeader(EthDevice& eth, udp::PacketInfo& pkt)
{
	uint32_t data = eth.rx.payload();
	eth.rx.pop_unsafe();

	for (int i = 0; i < 3; ++i)
	{
		uint32_t next = eth.rx.payload();
		eth.rx.pop_unsafe();

		pkt.mac[i] = data >> 16 | next << 16;
		data = next;
	}

	return uint16_t(data >> 16);
}

void udp::prepareEthHeader(EthDevice& eth, PacketInfo& pkt, EthCrc32& crc)
{
	auto push = [&](uint32_t val) { eth.tx.push(val); crc.add<32>(val); };

	// insert nic tag in first beat
	crc.add<16>(pkt.mac[1] >> 16);
	eth.tx.push(pkt.mac[1] & 0xFFFF'0000);

	push(pkt.mac[2]);
	push(pkt.mac[0]);
	push(pkt.mac[1] & 0xFFFF | 0x8'0000); // IPv4
}

uint16_t udp::parseIpHeader(EthDevice& eth, udp::PacketInfo& pkt)
{
	uint32_t a = parsePop(eth);
	if ((a >> 4 & 0xf) != 4) // IPv4
		return 0;
	if ((a & 0xF) < 5) // minimal length
		return 0;

	parsePop(eth);
	uint32_t c = parsePop(eth);
	pkt.ip[0] = parsePop(eth);
	pkt.ip[1] = parsePop(eth);

	for (uint32_t i = 5; i < (a & 0xF); ++i)
		parsePop(eth);

	return c >> 8 & 0xFF;
}

void udp::prepareIpHeader(EthDevice& eth, PacketInfo& pkt, EthCrc32& crc, size_t payloadLengthBytes)
{
	auto push = [&](uint32_t val) { eth.tx.push(val); crc.add<32>(val); };

	uint32_t totalLength = ntoh(uint16_t(payloadLengthBytes + 20 + 8));
	uint32_t ttl = 16;
	uint32_t protocol = 17;

	uint32_t word0 = 0x45 | totalLength << 16;
	uint32_t word2 = ttl | protocol << 8;
	uint32_t checksum = checksumIp({ word0, word2, pkt.ip[0], pkt.ip[1] });

	push(word0);
	push(0);
	push(word2 | checksum << 16);
	push(pkt.ip[1]);
	push(pkt.ip[0]);
}

uint16_t udp::parseUdpHeader(EthDevice& eth, udp::PacketInfo& pkt)
{
	pkt.port = parsePop(eth);
	parsePop(eth);

	return ntoh(uint16_t(pkt.port >> 16));
}

void udp::prepareUdpHeader(EthDevice& eth, PacketInfo& pkt, EthCrc32& crc, size_t payloadLengthBytes)
{
	auto push = [&](uint32_t val) { eth.tx.push(val); crc.add<32>(val); };
	push(pkt.port >> 16 | pkt.port << 16);

	uint32_t length = ntoh(uint16_t(payloadLengthBytes + 8));
	push(length);
}

uint16_t udp::parseUdpPacket(EthDevice& eth, udp::PacketInfo& pkt)
{
	uint16_t ethProto = parseEthHeader(eth, pkt);
	if (ethProto == 8)
	{
		uint16_t ipProto = parseIpHeader(eth, pkt);
		if (ipProto == 17)
		{
			return parseUdpHeader(eth, pkt);
		}
	}
	return false;
}

void udp::sendUdpPacket(EthDevice& eth, PacketInfo& pkt, std::span<const uint32_t> payload)
{
	EthCrc32 crc;
	eth.tx.eop(false);
	prepareEthHeader(eth, pkt, crc);
	prepareIpHeader(eth, pkt, crc, payload.size_bytes());
	prepareUdpHeader(eth, pkt, crc, payload.size_bytes());

	for (uint32_t v : payload)
	{
		eth.tx.push(v);
		crc.add<32>(v);
	}
	for (size_t i = payload.size(); i < 5; ++i)
	{
		eth.tx.push(0);
		crc.add<32>(0);
	}

	eth.tx.eop(true);
	eth.tx.push(crc.finalize());
	eth.tx.commit();
	eth.tx.eop(false);
}

void udp::PacketHandler::processPacket(EthDevice& eth, uint16_t listenPort)
{
	if (!eth.rx.valid())
		return;

	EthCrc32 crc;
	PacketInfo pkt;
	if (parseUdpPacket(eth, pkt) == listenPort)
	{
		uint32_t len = 0;
		uint32_t packet_buffer[512];
		while (!eth.rx.eop())
			packet_buffer[len++] = parsePop(eth);

		onPacket(pkt, packet_buffer, len);
	}
	while (!eth.rx.eop())
		eth.rx.pop();
	eth.rx.pop();
	eth.rx.commit();
}
