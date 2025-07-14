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
#include "HyperRAM.h"

namespace gtry::scl
{
	void hyperRAMSimulationModel(HyperBusPhy& phy, HyperRAMConfig cfg)
	{
#if 0
		const Clock& sysClk = ClockScope::getClk();

		Clock ramClkFree = sysClk.deriveClock({ 
			.phaseShiftDeg = 90, 
			.name = "HyperRAMClockFree", 
			.triggerEvent = ClockConfig::TriggerEvent::RISING_AND_FALLING 
		});
		Clock ramClk = ramClkFree.deriveClock({ .name = "HyperRAMClock" });
		ramClk.overrideClkWith(ramClkFree.clkSignal() & allowClockDomainCrossing(phy.cken, sysClk, ramClkFree));

		ClockScope ramClkScope(ramClk);
		//HyperBusPhy ramPhy = allowClockDomainCrossing(phy, sysclk, ramClock);

		HyperBusPhy ramPhy = constructFrom(phy);
		ramPhy.cs = allowClockDomainCrossing(phy.cs, sysClk, ramClk);
		ramPhy.reset = allowClockDomainCrossing(phy.reset, sysClk, ramClk);
		ramPhy.cken = '1'; // this is baked into ramClk

		ramPhy.rwds.f = allowClockDomainCrossing(phy.rwds.f, sysClk, ramClk);
		ramPhy.rwds.fen = allowClockDomainCrossing(phy.rwds.fen, sysClk, ramClk);
		*phy.rwds.r = allowClockDomainCrossing(*ramPhy.rwds.r, ramClk, sysClk);
		*phy.rwds.ren = allowClockDomainCrossing(*ramPhy.rwds.ren, ramClk, sysClk);

		ramPhy.dq.f = allowClockDomainCrossing(phy.dq.f, sysClk, ramClk);
		ramPhy.dq.fen = allowClockDomainCrossing(phy.dq.fen, sysClk, ramClk);
		*phy.dq.r = allowClockDomainCrossing(*ramPhy.dq.r, ramClk, sysClk);
		*phy.dq.ren = allowClockDomainCrossing(*ramPhy.dq.ren, ramClk, sysClk);

		hyperRAMSimulationModelClockShifted(ramPhy, cfg);
	}

	void hyperRAMSimulationModelClockShifted(HyperBusPhy& phy, HyperRAMConfig cfg)
	{
		Area ent{ "scl_HyperRAMModel", true };

		*phy.rwds.r = '0';
		*phy.rwds.ren = '0';
		*phy.dq.r = undefined(*phy.dq.r);
		*phy.dq.ren = '0';

		// TODO:
		// zero latency register write
		// register read
		// dynamic latency
		// Wrapped bursts
		// stall reads on boundaries
		// reset

		sim_assert(phy.reset == '0') << "reset not implemented";

		enum class State
		{
			command_address,	// wait for command and address transfer
			wait2,				// wait in case of extended 2x latency
			wait1,				// wait normal latency (2 cycles shorter than wait2)
			write,				// receive write data
			read,				// send read data
		};

		Bit chipSelect = phy.cs.at(cfg.csIndex) | name("chipSelect");

		// state and counter need asynchronous reset since clock may stop during !cs
		Clock ramClk = ClockScope::getClk();
		Clock frontendClk = ramClk.deriveClock({ .resetType = ClockConfig::ResetType::ASYNCHRONOUS, .resetActive = ClockConfig::ResetActive::LOW });
		frontendClk.overrideRstWith(chipSelect);

		ClockScope frontendScope{ frontendClk };
		Reg<Enum<State>> state{ State::command_address };
		state.setName("state");

		scl::Counter ctr{ 16 };
		IF(state.next() != state.current() | !chipSelect)
			ctr.reset();

		ClockScope ramScope{ ramClk };

		BVec command = 48_b;
		UInt address = cfg.rowAddrW + cfg.colAddrW + 1_b;
		IF(state.current() == State::command_address)
		{
			IF(chipSelect)
			{
				*phy.rwds.r = '1';
				*phy.rwds.ren = '1';

				IF(phy.cken)
				{
					ctr.inc();
					command.upper(-8_b) = command.lower(-8_b);
					command.lower(8_b) = phy.dq.f;

					IF(ctr.value() == 5)
					{
						sim_assert(command[46] == '0') << "register access not implemented";
						sim_assert(command[45] == '1') << "wrapped bursts not implemented";
						
						state = State::wait2;
					}
				}
			}

			address = cat(command(16, address.width() - 4_b), command.lower(3_b), Bit{'0'});
		}
		command = reg(command) | name("command");
		address = reg(address) | name("address");

		IF(state.current() != State::command_address)
		{
			IF(command[47] == '1') // keep rwds going for reads
				*phy.rwds.ren = '1';
		}

		UInt latency = ctr.value().width();
		latency = reg(latency, cfg.latencyDefault * 2 - 1) | name("latency");

		IF(state.current() == State::wait2)
		{
			ctr.inc();
			IF(ctr.value() == latency)
				state = State::wait1;
		}

		IF(state.current() == State::wait1)
		{
			ctr.inc();
			IF(ctr.value() == latency - 2)
			{
				IF(command[47])
					state = State::read;
				ELSE
					state = State::write;
			}
		}

		Memory<BVec> memory(address.width().count(), 8_b);
		memory.setType(MemType::DONT_CARE, 0);
		memory.noConflicts();
		
		auto memoryPort = memory[address];
		BVec memoryWord = memoryPort.read() | name("memoryWord");

		IF(state.current() == State::write)
		{
			IF(chipSelect & phy.cken)
			{
				IF(phy.rwds.f)
					memoryPort.write(phy.dq.f);

				address += 1;
			}
		}

		Bit requestReadBus = '0';
		IF(state.current() == State::read)
		{
			IF(chipSelect)
			{
				requestReadBus = '1';
				//*phy.rwds.ren = '1';
				//*phy.dq.ren = '1';

				IF(phy.cken)
					address += 1;
			}
		}

		const size_t CKDShift = cfg.tCKD * 360 / frontendClk.period();
		const size_t outShift = 180 + CKDShift;
		Clock outDelayClk = frontendClk.deriveClock({ .phaseShiftDeg = outShift % 180, .name = "HyperRAMModelOutClk" });
		auto delayReadSignal = [&](auto&& in) {
			auto dly = allowClockDomainCrossing(in, frontendClk, outDelayClk);
			for (size_t i = 0; i < (outShift + 179) / 180; ++i)
				dly = reg(dly, allZeros(in), {.clock = outDelayClk});
			dly = allowClockDomainCrossing(dly, outDelayClk, frontendClk);
			return dly;
		};

		*phy.dq.r = delayReadSignal(memoryWord);

		IF(delayReadSignal(requestReadBus))
		{
			*phy.rwds.r = ~allowClockDomainCrossing(outDelayClk.clkSignal(), outDelayClk, ramClk);
			*phy.dq.ren = '1';
		}

		IF(!chipSelect)
		{
			*phy.dq.ren = '0';
			*phy.rwds.ren = '0';
			state = State::command_address;
		}
#endif
	}
}
