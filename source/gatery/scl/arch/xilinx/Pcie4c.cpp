/*
HashCache, a rtl implementation and supporting software code for the hashcache datastructure.
Copyright (C) 2023 Synogate UG (haftungsbeschraenkt), www.synogate.com

This program is commercial software: you must not copy, modify or
distribute it without explicit written license by Synogate to do so.

This program is distributed WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY.
*/
#include <gatery/scl_pch.h>
#include "Pcie4c.h"


using namespace gtry;
using namespace gtry::scl;

namespace gtry::scl::arch::xilinx {
	using namespace scl::pci::xilinx;

	Pcie4c::Pcie4c(const Clock& ipClock, const Clock& gtClock,  Settings cfg, std::string_view name):
		ExternalModule(name, "xil_defaultlib"),
		m_usrClk(clockOut("user_clk", "user_reset", ClockConfig{
		.absoluteFrequency = hlim::ClockRational{ cfg.userClkFrequency, 1 },
		.name = "pcie_usr_clk",
		.resetType = ClockConfig::ResetType::ASYNCHRONOUS,
		.memoryResetType = ClockConfig::ResetType::NONE,
		.initializeMemory = true,
		.resetActive = ClockConfig::ResetActive::HIGH,
			})),
			m_cfg(cfg)
	{
		clockIn(ipClock, "sys_clk");
		clockIn(gtClock, "sys_clk_gt");
	
		{
			ClockScope clkScope{ ipClock };
			in("sys_reset") = ipClock.rstSignal();
		}
		buildSignals();
	}
	
	Axi4PacketStream<CQUser> Pcie4c::completerRequest()
	{
		ClockScope clkScope{ m_usrClk };
		Axi4PacketStream<CQUser> cq;
	
		const PinConfig pinCfg{ .type = PinType::STD_LOGIC };
	
		*cq = out("m_axis_cq_tdata", m_cfg.dataBusW);
		cq.template get<CQUser>() = { out("m_axis_cq_tuser", m_cfg.dataBusW == 512_b? 183_b: 88_b) };
	
		valid(cq) = out("m_axis_cq_tvalid");
		dwordEnable(cq) = out("m_axis_cq_tkeep", BitWidth(m_cfg.dataBusW.bits()/32));
		eop(cq)	= out("m_axis_cq_tlast");
	
		in("m_axis_cq_tready") = ready(cq);
	
		return cq;
	}
	
	Pcie4c& Pcie4c::completerCompletion(Axi4PacketStream<CCUser>&& cc) 
	{
		ClockScope clk{ m_usrClk };
	
		in("s_axis_cc_tdata", m_cfg.dataBusW) = *cc;
		in("s_axis_cc_tkeep", BitWidth(m_cfg.dataBusW.bits()/32)) = dwordEnable(cc);
	
		in("s_axis_cc_tlast") = eop(cc);
		in("s_axis_cc_tuser", m_cfg.dataBusW == 512_b? 81_b: 33_b) = cc.template get<CCUser>().raw;
		in("s_axis_cc_tvalid") = valid(cc);
	
		ready(cc) = out("s_axis_cc_tready", 4_b).lsb(); //all 4 bits indicate the same value, "any of the bit" can be used
	
		return *this;
	}
	
	Axi4PacketStream<RCUser> Pcie4c::requesterCompletion() 
	{
		ClockScope clkScope{ m_usrClk };
		Axi4PacketStream<RCUser> rc;
	
		*rc = out("m_axis_rc_tdata", m_cfg.dataBusW);
		rc.template get<RCUser>() = { out("m_axis_rc_tuser", m_cfg.dataBusW == 512_b ? 161_b : 75_b) };
	
		valid(rc) = out("m_axis_rc_tvalid");
		dwordEnable(rc) = out("m_axis_rc_tkeep", BitWidth(m_cfg.dataBusW.bits()/32));
		eop(rc)	=	out("m_axis_rc_tlast");
	
		in("m_axis_rc_tready") = ready(rc);
	
		return rc;
	}
	
	Pcie4c& Pcie4c::requesterRequest(Axi4PacketStream<RQUser>&& rq)
	{
		ClockScope clk{ m_usrClk };
	
		in("s_axis_rq_tdata", m_cfg.dataBusW) = *rq;
		in("s_axis_rq_tkeep", BitWidth(m_cfg.dataBusW.bits()/32)) = dwordEnable(rq);
	
		in("s_axis_rq_tlast") = eop(rq);
		in("s_axis_rq_tuser", m_cfg.dataBusW == 512_b ? 137_b : 62_b) = rq.template get<RQUser>().raw;
		in("s_axis_rq_tvalid") = valid(rq);
	
		ready(rq) = out("s_axis_rq_tready", 4_b).lsb();
	
		return *this;
	}
	
	void Pcie4c::buildSignals()
	{
		BVec& rxn =  in("pci_exp_rxn", BitWidth(m_cfg.lanes));
		BVec& rxp =  in("pci_exp_rxp", BitWidth(m_cfg.lanes));
		BVec txn = out("pci_exp_txn", BitWidth(m_cfg.lanes));
		BVec txp = out("pci_exp_txp", BitWidth(m_cfg.lanes));
		
		rxp = 0;
		rxn = 0;
		for (size_t i = 0; i < m_cfg.lanes; i++) {
			 pinIn(rxn.at(i), std::string(m_cfg.PinRx) + std::to_string(i) + "_N");
			 pinIn(rxp.at(i), std::string(m_cfg.PinRx) + std::to_string(i) + "_P");
			 pinOut(txn.at(i), std::string(m_cfg.PinTx) + std::to_string(i) + "_N");
			 pinOut(txp.at(i), std::string(m_cfg.PinTx) + std::to_string(i) + "_P");
		}
	
		ClockScope clk{ m_usrClk };

		m_status.user_lnk_up = out("user_lnk_up");
		m_status.phy_rdy_out = out("phy_rdy_out");
	
		in("cfg_interrupt_int"    , 4_b) = 0;
		in("cfg_interrupt_pending", 4_b) = 0;

		in("m_axis_cq_tready") = '0';
		
		in("s_axis_cc_tvalid") = '0';
		in("s_axis_cc_tdata", m_cfg.dataBusW) = 0;
		in("s_axis_cc_tkeep", BitWidth(m_cfg.dataBusW.bits()/32)) = 0;
		in("s_axis_cc_tlast") = '0';
		in("s_axis_cc_tuser", m_cfg.dataBusW == 512_b ? 81_b : 33_b) = 0;

		in("m_axis_rc_tready") = '0';

		in("s_axis_rq_tvalid") = '0';
		in("s_axis_rq_tdata", m_cfg.dataBusW) = 0;
		in("s_axis_rq_tkeep", BitWidth(m_cfg.dataBusW.bits()/32)) = 0;
		in("s_axis_rq_tlast") = '0';
		in("s_axis_rq_tuser", m_cfg.dataBusW == 512_b ? 137_b : 62_b) = 0;
	}
}
