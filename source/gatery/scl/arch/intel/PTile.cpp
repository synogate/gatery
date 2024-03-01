/*
HashCache, a rtl implementation and supporting software code for the hashcache datastructure.
Copyright (C) 2023 Synogate UG (haftungsbeschraenkt), www.synogate.com

This program is commercial software: you must not copy, modify or
distribute it without explicit written license by Synogate to do so.

This program is distributed WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY.
*/

#include "gatery/pch.h"
#include "PTile.h"

using namespace gtry;
using namespace gtry::scl;
namespace gtry::scl::arch::intel {
	PTile::PTile(std::string_view name, Settings cfg):
		ExternalModule(name, name),
		m_usrClk(clockOut("coreclkout_hip", "p0_pin_perst_n", ClockConfig{
		.absoluteFrequency = hlim::ClockRational{ cfg.userClkFrequency, 1 },
		.name = "pcie_usr_clk",
		.resetType = ClockConfig::ResetType::ASYNCHRONOUS,
		.memoryResetType = ClockConfig::ResetType::NONE,
		.initializeMemory = true,
		.resetActive = ClockConfig::ResetActive::LOW,
			})),
			m_cfg(cfg)
	{
		buildSignals();
	}




	PTile& PTile::tx(RvPacketStream<BVec, scl::Error, PTileHeader, PTilePrefix>&& stream)
	{
		ClockScope clk{ m_usrClk };

		const PinConfig pinCfg{ .type = PinType::STD_LOGIC  };

		//in("tx_data_i"	, 128 * m_cfg.busIndexVariable_w).word(interfaceNumber, 128 * m_cfg.busIndexVariable_w / m_cfg.numberOfInterfaces) = *stream;
		in("p0_tx_st_data_i", m_cfg.dataBusW) = *stream;

		in("p0_tx_st_hdr_i", 128_b) = (BVec) pack(stream.get<PTileHeader>());
		in("p0_tx_st_tlp_prfx_i", 32_b) = stream.get<PTilePrefix>().prefix;
		;
		in("p0_tx_st_valid_i", 1_b).lsb() = valid(stream);
		in("p0_tx_st_sop_i", 1_b).lsb() = sop(stream);
		in("p0_tx_st_eop_i", 1_b).lsb() = eop(stream);
		in("p0_tx_st_err_i", 1_b).lsb() = error(stream);

		Bit exportReady = out("p0_tx_st_ready_o"); 
		for (size_t i = 0; i < m_cfg.txReadyLatency; ++i)
			exportReady = reg(exportReady, '0');


		ready(stream).exportOverride(exportReady);

		return *this;
	}

	RvPacketStream<BVec, EmptyBits, PTileHeader,  PTilePrefix, PTileBarRange> PTile::rx()
	{
		ClockScope clkScope{ m_usrClk };
		RvPacketStream<BVec, EmptyBits,  PTileHeader,  PTilePrefix, PTileBarRange> rx;

		const PinConfig pinCfg{ .type = PinType::STD_LOGIC };

		*rx = out("p0_rx_st_data_o", m_cfg.dataBusW);
		unpack(out("p0_rx_st_hdr_o", 128_b), rx.get<PTileHeader>());
		unpack(out("p0_rx_st_tlp_prfx_o", 32_b), rx.get<PTilePrefix>());
		rx.get<PTileBarRange>() = PTileBarRange{ out("p0_rx_st_bar_range_o", 3_b) };

		//p0_rx_st_tlp_abort_o : does not apply to non-bypass tlp mode
		//p0_rx_par_err_o

		valid(rx) = out("p0_rx_st_valid_o", 1_b).lsb();
		emptyBits(rx) = (UInt)cat(out("p0_rx_st_empty_o", BitWidth::count(m_cfg.dataBusW.bits() / 32)), UInt{"5d0"});
		/*sop(rx) =*/ out("p0_rx_st_sop_o", 1_b).lsb();
		eop(rx)	=	out("p0_rx_st_eop_o", 1_b).lsb();

		in("p0_rx_st_ready_i") = ready(rx);

		return rx;
	}

	void PTile::buildSignals()
	{
		const PinConfig pinCfg{ .type = PinType::STD_LOGIC };

		m_outputConfig.func	= out("p0_tl_cfg_func_o", 3_b);
		m_outputConfig.Addr	= out("p0_tl_cfg_add_o" , 5_b);
		m_outputConfig.Ctl	= out("p0_tl_cfg_ctl_o" ,16_b);

		m_status.dataLinkTimerUpdate = out("p0_dl_timer_update_o");
		m_status.resetStatusN = out("p0_reset_status_n");
		m_status.pinPerstN = out("p0_pin_perst_n");
		m_status.linkUp = out("p0_link_up_o");
		m_status.dataLinkUp = out("p0_dl_up_o");
		m_status.surpriseDownError = out("p0_surprise_down_err_o");
		m_status.ltssmState = out("p0_ltssm_state_o", 6_b);

		for (size_t i = 0; i < m_cfg.lanes; i++) pinIn(in("rx_n_in" + std::to_string(i)), std::string(m_cfg.PinRxN) + std::to_string(i));
		for (size_t i = 0; i < m_cfg.lanes; i++) pinIn(in("rx_p_in" + std::to_string(i)), std::string(m_cfg.PinRxP) + std::to_string(i));
		for (size_t i = 0; i < m_cfg.lanes; i++) pinOut(out("tx_n_out" + std::to_string(i)), std::string(m_cfg.PinTxN) + std::to_string(i));
		for (size_t i = 0; i < m_cfg.lanes; i++) pinOut(out("tx_p_out" + std::to_string(i)), std::string(m_cfg.PinTxP) + std::to_string(i));

		pinIn(in("refclk0"), std::string(m_cfg.PinRefClk0P));
		pinIn(in("refclk1"), std::string(m_cfg.PinRefClk1P));
		pinIn(in("pin_perst_n"), std::string(m_cfg.PinPerstN));		
	}
}
