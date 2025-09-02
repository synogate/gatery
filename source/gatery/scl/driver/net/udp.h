#pragma once
#include "../devices/ethernet.h"
#include <span>
#include <cstdint>
#include <stddef.h>

namespace udp
{
	void packetLoopback(EthDevice* dev);

	struct EthCrc32
	{
		template<int N>
		void add(uint32_t val)
		{
			state_ ^= val;
			for (int i = 0; i < N; ++i)
			{
				int32_t mask = int32_t(state_) << 31 >> 31;
				state_ >>= 1;
				state_ ^= mask & 0xEDB88320;// 0x04C11DB7;
			}
		}

		void operator() (uint32_t val) { add<32>(val); }

		uint32_t finalize() const
		{
			return ~state_;
		}

	private:
		uint32_t state_ = 0xFFFFFFFFul;
	};

	uint16_t checksumIp(std::initializer_list<uint32_t> list);

	struct PacketInfo
	{
		uint32_t mac[3];
		uint32_t ip[2];
		uint32_t port;
	};

	inline uint32_t parsePop(EthDevice& eth) { uint32_t ret = eth.rx.payload(); eth.rx.pop_unsafe(); return ret; }

	// returns layer 3 protocol
	uint16_t parseEthHeader(EthDevice& eth, PacketInfo& pkt);
	void prepareEthHeader(EthDevice& eth, PacketInfo& pkt, EthCrc32& crc);

	// returns layer 4 protocol
	uint16_t parseIpHeader(EthDevice& eth, PacketInfo& pkt);
	void prepareIpHeader(EthDevice& eth, PacketInfo& pkt, EthCrc32& crc, size_t payloadLengthBytes);

	// return destination port
	uint16_t parseUdpHeader(EthDevice& eth, PacketInfo& pkt);
	void prepareUdpHeader(EthDevice& eth, PacketInfo& pkt, EthCrc32& crc, size_t payloadLengthBytes);

	// returns destination port
	uint16_t parseUdpPacket(EthDevice& eth, PacketInfo& pkt);
	void sendUdpPacket(EthDevice& eth, PacketInfo& pkt, std::span<const uint32_t> payload);

	inline uint16_t ntoh(uint16_t v) { return v >> 8 | v << 8; }
	inline uint32_t ntoh(uint32_t v) { return v >> 24 & 0xFF | v >> 8 & 0xFF | v << 8 & 0xFF | v << 24 & 0xFF; }

	class PacketHandler
	{
	public:
		void processPacket(EthDevice& eth, uint16_t listenPort);

	protected:
		virtual void onPacket(PacketInfo& pkt, uint32_t* buffer, size_t lengthWords) {}
	};
}
