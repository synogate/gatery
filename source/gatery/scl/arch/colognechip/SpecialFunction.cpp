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
#include "gatery/scl_pch.h"
#include "SpecialFunction.h"
#include "io.h"
#include "lut.h"

#include "../../Counter.h"
#include "../../flag.h"
#include "../../cdc.h"

namespace gtry::scl::arch::colognechip
{
	CC_BUFG::CC_BUFG() : ExternalModule("CC_BUFG") {
		requiresComponentDeclaration(true);
		isEntity(false);
	}

	Bit bufg(Bit I) 
	{ 
		CC_BUFG buf; 
		buf.I() = I; 
		return buf.O(); 
	}

	CC_USR_RSTN::CC_USR_RSTN() : ExternalModule("CC_USR_RSTN") {
		requiresComponentDeclaration(true);
		isEntity(false);
	}

	Bit usr_rstn() 
	{ 
		return CC_USR_RSTN{}.usr_rstn(); 
	}

	Clock globalReset(const Clock& clk)
	{
		Clock out = clk.deriveClock({ 
			.resetName = std::string(clk.name()) + "_usr_rstn",
			.resetType = ClockConfig::ResetType::ASYNCHRONOUS, 
			.resetActive = ClockConfig::ResetActive::LOW,
		});
		out.reset(usr_rstn(), ClockScope::getClk(), ClockConfig::ResetActive::LOW);
		return out;
	}

	CC_PLL::CC_PLL() : ExternalModule("CC_PLL") {
		requiresComponentDeclaration(true);
		isEntity(false);

		// we need to set filter to reduce the amount of warnings from p_r
		filter();
	}

	CC_PLL& CC_PLL::clkRef(const Clock& ref, bool user)
	{
		std::string_view portName = user ? "USR_CLK_REF" : "CLK_REF";
		clockIn(ref, portName, "USR_LOCKED_STDY_RST");
		refClkFreq(ref.absoluteFrequency());

		std::string_view unusedName = !user ? "USR_CLK_REF" : "CLK_REF";
		in(unusedName);

		return *this;
	}

	Clock CC_PLL::clkOut(const ClockConfig& cfg, bool bufg)
	{
		HCL_DESIGNCHECK(cfg.absoluteFrequency);
		outClkFreq(*cfg.absoluteFrequency);

		std::string portName = "CLK0";
		if (cfg.phaseShiftDeg)
		{
			HCL_DESIGNCHECK_HINT(*cfg.phaseShiftDeg % 90 == 0, "the requested phase shift is not supported by pll");
			portName = "CLK" + std::to_string(*cfg.phaseShiftDeg % 360);
		}

		Clock clk = clockOut(portName, {}, cfg);
		if (cfg.resetType && *cfg.resetType != hlim::RegisterAttributes::ResetType::NONE)
			clk.reset(out("USR_PLL_LOCKED"), ClockScope::getClk(), ClockConfig::ResetActive::LOW);

		if (bufg)
		{
			Bit clockDummy; // has to be unassigned for simulation magic :/
			clockDummy.exportOverride(colognechip::bufg(out(portName)));
			clk.overrideClkWith(clockDummy);
		}
		return clk;
	}

	CC_PLL& CC_PLL::filter(size_t integralCoefficient, size_t proportionalCoefficient)
	{
		generic("CI_FILTER_CONST") = integralCoefficient;
		generic("CP_FILTER_CONST") = proportionalCoefficient;
		return *this;
	}

	CC_PLL& CC_PLL::outClkFreq(hlim::ClockRational freq)
	{
		double mhz = hlim::toDouble(freq / hlim::ClockRational{ 1'000'000 });
		generic("OUT_CLK") = std::to_string(mhz);
		return *this;
	}

	CC_PLL& CC_PLL::refClkFreq(hlim::ClockRational freq)
	{
		double mhz = hlim::toDouble(freq / hlim::ClockRational{ 1'000'000 });
		generic("REF_CLK") = std::to_string(mhz);
		return *this;
	}

	Clock pll(const Clock& src, const ClockConfig& cfg, bool userClk)
	{
		ClockScope c{ src };
		return CC_PLL{}
			.lockRequired(true)
			.clkRef(ClockScope::getClk(), userClk)
			.clkOut(cfg);
	}

	Clock clkdiv(const Clock& src, ClockConfig cfg)
	{
		if (!cfg.absoluteFrequency)
		{
			// we convert to absolute frequency to signal no phase relationsship to gatery
			HCL_DESIGNCHECK(cfg.frequencyMultiplier);
			cfg.absoluteFrequency = src.absoluteFrequency() * *cfg.frequencyMultiplier;
			cfg.frequencyMultiplier.reset();
		}

		hlim::ClockRational mul = *cfg.absoluteFrequency / src.absoluteFrequency();
		HCL_DESIGNCHECK_HINT(mul.numerator() == 1, "can only divide, not multiply. use pll to multiply.");

		size_t divider = mul.denominator();
		if (divider == 1)
		{
			cfg.absoluteFrequency.reset();
			return src.deriveClock(cfg);
		}

		ClockScope clkscope{ src };
		Bit clkSignal;
		if (divider % 2 == 0)
		{
			clkSignal = reg(clkSignal ^ scl::Counter(divider / 2).isLast(), '0');
		}
		else
		{
			Clock risingClk = src.deriveClock({ .triggerEvent = ClockConfig::TriggerEvent::RISING });;
			Clock fallingClk = src.deriveClock({ .triggerEvent = ClockConfig::TriggerEvent::FALLING });

			ClockScope rising{ risingClk };
			scl::Counter ctr1(divider);
			Bit clk1 = scl::flag(ctr1.isLast(), ctr1.value() == divider / 2 - 1) | name("clk1");

			ClockScope falling{ fallingClk };
			scl::Counter ctr2(divider);
			Bit clk2 = scl::flag(ctr2.isLast(), ctr2.value() == divider / 2 - 1) | name("clk2");

			clkSignal = clk1 | clk2;
		}
		
		CC_BUFG buf;
		buf.I() = clkSignal;
		Clock slowClk = buf.clockOut("O", {}, cfg);

		if (slowClk.getClk()->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE)
		{
			HCL_DESIGNCHECK(src.getClk()->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE);
			slowClk.reset(src.rstSignal(), src.getClk(), src.getClk()->getRegAttribs().resetActive);
		}
		return slowClk;
	}

	Clock clkdivRegChain(const Clock& src, size_t log2div, bool buffer)
	{
		Clock clk = src;
		for (size_t i = 0; i < log2div; ++i)
		{
			ClockScope cs{ clk };

			Bit nextClk;
			nextClk = reg(!nextClk, '0');
			clk = clk.deriveClock({
				.frequencyMultiplier = hlim::ClockRational{1,2},
				.name = std::format("{}_div{}", src.name(), 2ull << i),
			});

			Bit clkSig = nextClk;
			if (buffer && i == log2div - 1)
				clkSig = bufg(clkSig) | name(std::string{ clk.name() } + "_sig");

			Bit clockDummy; // has to be unassigned for simulation magic :/
			clockDummy.exportOverride(clkSig);
			clk.overrideClkWith(clockDummy);
		}
		return clk;
	}

	void configure(RvPacketStream<BVec>&& bitstream, CC_CFG_CTRL& ctrl)
	{
		Area ent{ "scl_gatemate_configure", true };

		enum class State
		{
			idle, 
			wait_enable,
			send,
			wait_gap,
			wait_end,
		};
		Reg<Enum<State>> state{ State::idle };
		state.setName("state");

		scl::Counter stateCtr(8);
		IF(state.current() != state.next())
			stateCtr.reset();

		ready(bitstream) = '0';

		IF(state.current() == State::idle)
			IF(valid(bitstream))
				state = State::wait_enable;

		IF(state.current() == State::wait_enable)
			IF(stateCtr.value() == 2)
				state = State::send;

		ctrl.valid() = '0';
		IF(state.current() == State::send)
		{
			ctrl.valid() = valid(bitstream);
			ready(bitstream) = '1';
			IF(transfer(bitstream))
			{
				state = State::wait_gap;
				IF(eop(bitstream))
					state = State::wait_end;
			}
		}

		IF(state.current() == State::wait_gap)
			state = State::send;

		IF(state.current() == State::wait_end)
			IF(stateCtr.value() == 7)
				state = State::idle;

		ctrl.valid() = name(ctrl.valid(), "cfg_ctrl_valid");
		ctrl.en() = (state.current() != State::idle) | name("cfg_ctrl_en");
		ctrl.data() = *bitstream | name("cfg_ctrl_data");
	}

	Bit serdes(BVec data, const Clock& slowClk, const Clock& fastClk)
	{
		Area ent{ "scl_serdes_single", true };
		return serdes(data, slowClk, fastClk, scl::synchronizeEvent('1', slowClk, fastClk));
	}

	Bit serdes(BVec data, const Clock& slowClk, const Clock& fastClk, Bit syncPulse)
	{
		Area ent{ "scl_serdes", true };

		gtry::hlim::ClockRational ratio = fastClk.absoluteFrequency() / slowClk.absoluteFrequency();
		HCL_DESIGNCHECK(ratio.denominator() == 1);
		HCL_DESIGNCHECK(ratio.numerator() == data.size() || (data.size() % 2 == 0 && ratio.numerator() == data.size() / 2));
		const bool ddrMode = ratio.numerator() == data.size() / 2;

		if (data.width() == 1_b)
			return data.lsb();

		BVec inWord = scl::synchronize(data, slowClk, fastClk, { .outStages = 2, .inStage = false }) | name("inWord");

		BVec shiftReg = data.width();
		shiftReg >>= ddrMode ? 2 : 1;
		IF(reg(syncPulse))
			shiftReg = inWord;
		shiftReg = reg(shiftReg, { .clock = fastClk }) | name("shiftReg");

		if (ddrMode)
		{
			scl::arch::colognechip::CC_ODDR ddr;
			ddr.D0() = shiftReg[1];
			ddr.D1() = shiftReg[0];
			return ddr.Q();
		}
		else
		{
			return reg(shiftReg.lsb());
		}
	}

	CC_CFG_CTRL::CC_CFG_CTRL() : ExternalModule("CC_CFG_CTRL") {
		requiresComponentDeclaration(true);
		isEntity(false);
		hasSideEffects(true);

		clockIn("CLK");
		recfg() = '0';
		en() = '0';
		valid() = '0';
		data() = ConstBVec(8_b);
	}
}
