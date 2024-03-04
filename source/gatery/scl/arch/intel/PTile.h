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
#include <gatery/scl/arch/intel/IntelPci.h>

using namespace gtry;
using namespace gtry::scl;
using namespace gtry::scl::pci;

namespace gtry::scl::arch::intel {

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

		PTile(Settings cfg, std::string_view name = "ptile");

		RvPacketStream<BVec, scl::EmptyBits, PTileHeader, PTilePrefix, PTileBarRange> rx();
		PTile& tx(scl::strm::RvPacketStream<BVec, scl::Error, PTileHeader, PTilePrefix>&& stream);

		const Clock& userClock() { return m_usrClk; }
		void connectNInitDone(Bit ninit_done) { in("ninit_done", PinConfig{ .type = PinType::VL }) = ninit_done; }

		Settings settings() const { return m_cfg; }

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

}
