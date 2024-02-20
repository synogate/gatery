#pragma once
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
#pragma once

#include <gatery/frontend.h>
#include <gatery/scl/io/pci/pci.h>

using namespace gtry;
using namespace gtry::scl;
using namespace gtry::scl::pci;

namespace gtry::scl::arch::intel {

	struct Header {
		BVec header = 128_b;
	};

	struct Prefix {
		BVec prefix = 32_b;
	};

	struct BarRange {
		BVec encoding;
	};

	class PTile : public ExternalModule
	{
	public:
		struct Settings {
			size_t userClkFrequency;
			BitWidth dataBusW;
			size_t lanes;

			std::string_view PinPerstN= "fm6_pcie_perstn";
			std::string_view PinRefClk0P = "refclk_pcie_ch0_p";	
			std::string_view PinRefClk1P = "refclk_pcie_ch2_p";


			std::string_view PinTxP = "pcie_ep_tx_p";
			std::string_view PinRxP = "pcie_ep_rx_p";
			std::string_view PinTxN = "pcie_ep_tx_n";
			std::string_view PinRxN = "pcie_ep_rx_n";


			size_t txReadyLatency = 3;
			size_t txReadyAllowance = 3;
			size_t rxReadyLatency = 27;

		};

		struct Status {
			Bit resetStatusN;
			Bit pinPerstN;
			Bit linkUp;
			Bit dataLinkUp;
			Bit dataLinkTimerUpdate;
			Bit surpriseDownError;
			BVec ltssmState = 6_b;
		};

		struct OutputConfig {
			BVec Ctl;
			BVec Addr;
			BVec func;
		};

		PTile(std::string_view name, Settings cfg);

		RvPacketStream<BVec, scl::Empty, Header, Prefix, BarRange> rx();
		PTile& tx(scl::strm::RvPacketStream<BVec, scl::Error, Header, Prefix>&& stream);

		const Clock& userClock() { return m_usrClk; }
		void connectNInitDone(Bit ninit_done) { in("ninit_done", PinConfig{ .type = PinType::VL }) = ninit_done; }

	private:
		Settings m_cfg;
		Clock m_usrClk;
		std::string m_name;
		Status m_status;
		OutputConfig m_outputConfig;

		void buildSignals();


	public:
		struct Presets
		{
			static const Settings Gen3x16_256()
			{
				Settings ret;
				ret.userClkFrequency = 250'000'000;
				ret.dataBusW = 256_b;
				ret.lanes = 16;
				return ret;
			}

			static const Settings Gen3x16_512()
			{
				Settings ret;
				ret.userClkFrequency = 250'000'000;
				ret.dataBusW = 512_b;
				ret.lanes = 16;
				return ret;
			}
		};
	};

	TlpPacketStream<EmptyBits, BarRange> ptileRxVendorUnlocking(scl::strm::RvPacketStream<BVec, scl::Error, Header, Prefix, BarRange>&& rx);
	
	template<Signal... MetaT>
	scl::strm::RvPacketStream<BVec, scl::Error, Header, Prefix>&& ptileTxVendorUnlocking(TlpPacketStream<MetaT...>&& tx);

	//class PciInterfaceSeparator {
	//public:
	//	PciInterfaceSeparator();
	//
	//	template<Signal... MetaInT>
	//	PciInterfaceSeparator& attachRx(TlpPacketStream<MetaInT...>&& rx);
	//	
	//	template<Signal... MetaOutT>
	//	TlpPacketStream<MetaOutT...> getTx();
	//
	//	RequesterInterface& requesterInterface() { HCL_DESIGNCHECK(m_requesterInterface); return *m_requesterInterface; }
	//	CompleterInterface& completerInterface() { HCL_DESIGNCHECK(m_completerInterface); return *m_completerInterface; }
	//	
	//private:
	//	std::optional<RequesterInterface> m_requesterInterface;
	//	std::optional<CompleterInterface> m_completerInterface;
	//};
}

namespace gtry::scl::arch::intel {
	//template<Signal... MetaInT>
	//TlpPacketStream<EmptyBits, BarRange> rxVendorUnlocking(TlpPacketStream<MetaInT...>&& rx) {
	//	BitWidth rxW = rx->width();
	//	StreamBroadcaster rxCaster(move(rx));
	//	
	//	// Extract header into separate stream. this stream is a one beat, 128 bit packetStream.
	//	// clipboard: .transform([](const intel::Header& hdr)->BVec { return hdr.header; })
	//	Stream hdr = strm::extractMeta<intel::Header>(rxCaster.bcastTo());
	//	valid(hdr) &= sop(hdr); //must only be consumed once
	//	eop(hdr) = '1';
	//
	//	emptyBits(hdr) = 0; //4dw
	//	IF(pci::HeaderCommon::fromRawDw0(hdr->header.lower(32_b)).is3dw())
	//		emptyBits(hdr) = 32;
	//	
	//	//insert the header into the data stream
	//	return strm::insert(
	//		rxCaster.bcastTo(),
	//		strm::widthExtend(move(hdr), rxW),
	//		strm::createVStream<RvStream>(0, '1')
	//	)
	//		.reduceTo<TlpPacketStream<EmptyBits, BarRange>>() 
	//		| strm::regDownstream();
	//}
	//template<Signal... MetaInT>
	//auto txVendorUnlocking(TlpPacketStream<MetaInT...>&& tx) {
	//	HCL_DESIGNCHECK_HINT(tx->width() >= 128_b, "the width needs to be larger than 4DW for this implementation");
	//
	//	//add header as metaSignal
	//	BVec rawHdr = capture(tx->lower(128_b), valid(tx) & sop(tx));
	//	tx.add(Header{ rawHdr });
	//
	//	//remove header from front of TLP
	//	UInt headerSizeInBits = 128;
	//	IF(pci::HeaderCommon::fromRawDw0(rawHdr.lower(32_b)).is3dw())
	//		UInt headerSizeInBits = 96;
	//	tx = strm::streamShiftRight(move(tx), headerSizeInBits);
	//
	//}
}



