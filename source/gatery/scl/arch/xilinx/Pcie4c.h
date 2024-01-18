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
#include <gatery/scl/arch/xilinx/XilinxPci.h>

namespace gtry::scl::arch::xilinx 
{
	using namespace gtry::scl::pci::xilinx;
	class Pcie4c : public gtry::ExternalModule
	{
	public:
		struct Settings {
			size_t userClkFrequency = 250'000'000;
			BitWidth dataBusW;
			size_t lanes = 16;
	
			std::string_view PinTx = "PCIE_EP_TX";
			std::string_view PinRx = "PCIE_EP_RX";
		};
	
		struct Status {
			Bit user_lnk_up; 
			Bit phy_rdy_out;
		};
	
		Pcie4c(const Clock& ipClock, const Clock& gtClock, Settings cfg, std::string_view name = "pcie4c_uscale_plus_0");
	
		Axi4PacketStream<CQUser> completerRequest();
		Pcie4c& completerCompletion(Axi4PacketStream<CCUser>&& stream);
	
		Pcie4c& requesterRequest(Axi4PacketStream<RQUser>&& stream);
		Axi4PacketStream<RCUser> requesterCompletion();

		Settings settings() const { return m_cfg; }
	
		const Clock& userClock() const { return m_usrClk; } 
	private:
		Settings m_cfg;
		Clock m_usrClk;
		std::string m_name;
		Status m_status;
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

