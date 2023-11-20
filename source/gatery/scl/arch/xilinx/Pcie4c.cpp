/*
HashCache, a rtl implementation and supporting software code for the hashcache datastructure.
Copyright (C) 2023 Synogate UG (haftungsbeschraenkt), www.synogate.com

This program is commercial software: you must not copy, modify or
distribute it without explicit written license by Synogate to do so.

This program is distributed WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY.
*/

#include "Pcie4c.h"


using namespace gtry;
using namespace gtry::scl;

namespace gtry::scl::arch::xilinx {
	using namespace scl::pci::amd;

	Pcie4c::Pcie4c(std::string_view name, Bit ipClockInput, Bit gtClockInput,  Settings cfg):
		ExternalModule(name, ""),
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
		this->requiresComponentDeclaration(true);
		this->isEntity(false);

		in("sys_clk") = ipClockInput; 
		in("sys_clk_gt") = gtClockInput;

		buildSignals();
	}

	axi4PacketStream<CQUser> Pcie4c::completerRequest()
	{
		ClockScope clkScope{ m_usrClk };
		axi4PacketStream<CQUser> cq;

		const PinConfig pinCfg{ .type = PinType::STD_LOGIC };

		*cq = out("m_axis_cq_tdata", m_cfg.dataBusW);
		unpack(out("m_axis_cq_tuser", 183_b), cq.template get<CQUser>());

		valid(cq) = out("m_axis_cq_tvalid");
		keep(cq) = out("m_axis_cq_tkeep", BitWidth(m_cfg.dataBusW.bits()/32));
		eop(cq)	= out("m_axis_cq_tlast");

		in("m_axis_cq_tready") = ready(cq);

		return cq;
	}

	Pcie4c& Pcie4c::completerCompletion(axi4PacketStream<CCUser>&& cc)
	{
		ClockScope clk{ m_usrClk };

		in("s_axis_cc_tdata", m_cfg.dataBusW) = *cc;
		in("s_axis_cc_tkeep", BitWidth(m_cfg.dataBusW.bits()/32)) = keep(cc);

		in("s_axis_cc_tlast") = eop(cc);
		in("s_axis_cc_tuser", 81_b) = (BVec) pack(cc.template get<CCUser>());
		in("s_axis_cc_tvalid") = valid(cc);

		ready(cc) = out("s_axis_cc_tready", 4_b).lsb(); //all 4 bits indicate the same value, "any of the bit" can be used

		return *this;
	}

	axi4PacketStream<RCUser> Pcie4c::requesterCompletion()
	{
		ClockScope clkScope{ m_usrClk };
		axi4PacketStream<RCUser> rc;

		*rc = out("m_axis_rc_tdata", m_cfg.dataBusW);
		unpack(out("m_axis_rc_tuser", 161_b), rc.template get<RCUser>());

		valid(rc) = out("m_axis_rc_tvalid");
		keep(rc) = out("m_axis_rc_tkeep", BitWidth(m_cfg.dataBusW.bits()/32));
		eop(rc)	=	out("m_axis_rc_tlast");

		in("m_axis_rc_tready") = ready(rc);

		return rc;
	}

	Pcie4c& Pcie4c::requesterRequest(axi4PacketStream<RQUser>&& rq)
	{
		ClockScope clk{ m_usrClk };

		in("s_axis_rq_tdata", m_cfg.dataBusW) = *rq;
		in("s_axis_rq_tkeep", BitWidth(m_cfg.dataBusW.bits()/32)) = keep(rq);

		in("s_axis_rq_tlast") = eop(rq);
		in("s_axis_rq_tuser", 137_b) = (BVec)pack(rq.template get<RQUser>());
		in("s_axis_rq_tvalid") = valid(rq);

		ready(rq) = out("s_axis_rq_tready", 4_b).lsb();

		return *this;
	}

	void Pcie4c::buildSignals()
	{
		auto& rxn =  in("pci_exp_rxn", BitWidth(m_cfg.lanes));
		rxn = 0;
		auto& rxp =  in("pci_exp_rxp", BitWidth(m_cfg.lanes));
		rxp = 0;
		auto txn = out("pci_exp_txn", BitWidth(m_cfg.lanes));
		auto txp = out("pci_exp_txp", BitWidth(m_cfg.lanes));
		
		for (size_t i = 0; i < m_cfg.lanes; i++) {
			 pinIn(rxn.at(i), std::string(m_cfg.PinRx) + std::to_string(i) + "_N");
			 pinIn(rxp.at(i), std::string(m_cfg.PinRx) + std::to_string(i) + "_P");
			 pinOut(txn.at(i), std::string(m_cfg.PinTx) + std::to_string(i) + "_N");
			 pinOut(txp.at(i), std::string(m_cfg.PinTx) + std::to_string(i) + "_P");
		}

		m_status.user_lnk_up = out("user_lnk_up");
		m_status.phy_rdy_out = out("phy_rdy_out");

		in("cfg_interrupt_int"    , 4_b) = 0;
		in("cfg_interrupt_pending", 4_b) = 0;

		pinIn(in("sys_reset"), std::string(m_cfg.PinPerstN));
	}
}
