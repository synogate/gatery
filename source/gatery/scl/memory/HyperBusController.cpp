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
#include "HyperBusController.h"

#include <gatery/scl/arch/colognechip/io.h>

namespace gtry::scl
{
	Reverse<HyperBusPhy> gateMateHyperBusPhy(std::string pinPrefix, HyperBusPhyConfig cfg)
	{
		Area ent{ "scl_gateMateHyperBusPhy", true };

		Reverse<HyperBusPhy> phy;
		phy->csn = BitWidth{ cfg.pinCsn.size() };

		Bit anyActive = '0';
		for (const Bit& csn : phy->csn)
			anyActive |= !csn;
		HCL_NAMED(anyActive);

		// clock
		{
			scl::arch::colognechip::CC_ODDR clkDdr;
			clkDdr.D0() = anyActive;
			clkDdr.D1() = '0';

			if (cfg.pinClkN.empty())
			{
				// single ended clock
				pinOut(clkDdr.Q(), pinPrefix + std::string{ cfg.pinClk });
			}
			else
			{
				// LVDS clock
				arch::colognechip::CC_LVDS_OBUF clkBuf;
				clkBuf.pin(
					pinPrefix + std::string{ cfg.pinClk }, 
					pinPrefix + std::string{ cfg.pinClkN }
				);
				clkBuf.O() = clkDdr.Q();
			}
		}

		// chip select
		for(size_t i = 0; i < cfg.pinCsn.size(); ++i)
		{
			Bit csn;
			csn |= phy->csn[i]; // delayed reset
			csn = reg(csn, '1');
			csn &= phy->csn[i];	// instant set
			
			pinOut(csn, pinPrefix + std::string{ cfg.pinCsn[i] });
		}

#if 1
		// RWDS
		{
			arch::colognechip::CC_IOBUF buf;
			buf.pin(pinPrefix + std::string{ cfg.pinRwds });
			buf.delayOut(15);
			buf.disable() = !flagInstantSet(phy->rwds.fen, !phy->rwds.fen);

			arch::colognechip::CC_ODDR out(buf);
			out.D0() = phy->rwds.f.lsb();
			out.D1() = phy->rwds.f.msb();

			arch::colognechip::CC_IDDR in(buf);
			*phy->rwds.r = (BVec)pack(in.Q0(), reg(in.Q1()));
			*phy->rwds.r = simOverride(*phy->rwds.r, BVec{ "2b00" });
		}

		// DQ
		*phy->dq.r = ConstBVec(16_b);
		for (size_t i = 0; i < 8; ++i)
		{
			arch::colognechip::CC_IOBUF buf;
			buf.pin(pinPrefix + std::string{ cfg.pinDq } + std::to_string(i));
			buf.delayOut(15);
			buf.disable() = !flagInstantSet(phy->dq.fen, !phy->dq.fen);

			arch::colognechip::CC_ODDR out(buf);
			out.D0() = phy->dq.f[i];
			out.D1() = phy->dq.f[8 + i];

			arch::colognechip::CC_IDDR in(buf);
			(*phy->dq.r)[i] = in.Q0();
			(*phy->dq.r)[i + 8] = reg(in.Q1());
		}
#else
		*phy->rwds.r = 3;
		*phy->dq.r = 0xCCCC;
#endif
		// set defaults
		phy->csn = phy->csn.width().last();
		phy->rwds.f = ConstBVec(2_b);
		phy->rwds.fen = '0';
		phy->dq.f = ConstBVec(16_b);
		phy->dq.fen = '0';

		HCL_NAMED(phy);
		return phy;
	}

	Reverse<HyperBusController> hyperBusControllerLowLatency(Reverse<HyperBusPhy> phy)
	{
		Area ent{ "scl_hyperBusControllerLowLatency", true };
		HCL_NAMED(phy);

		enum class State
		{
			idle,				// wait for command
			command,			// transfer lower command words
			
			latency_initial,	// wait for initial latency to pass
			latency_additional,	// wait for addition latency to pass if needed

			write,				// transfer write data
			write_recover,		// wait for write data to leave phy and CS to be released

			read_latency,		// wait for data to transfer through phy
			read,				// transfer read data

			wait,
		};
		Reg<Enum<State>> state{ State::write_recover }; // we start in write_recover to allow chip select to be high for 2 cycles after por
		state.setName("state");

		Counter ctr(3_b);
		IF(state.next() != state.current())
			ctr.reset();

		Bit& csn = phy->csn.lsb();
		csn = '0';

		Reverse<HyperBusController> ctrl;
		ctrl->command->wordAddress = 32_b;

		// the first command word is issued instant
		phy->dq.f = 0;
		phy->dq.f(15, 1_b) = ctrl->command->type.toBVec();
		phy->dq.f(14, 1_b) = ctrl->command->space.toBVec();
		phy->dq.f(13, 1_b) = ctrl->command->burst.toBVec();

		const BVec wordAddr = (BVec)ctrl->command->wordAddress;
		phy->dq.f(0, 13_b) = wordAddr.upper(-19_b);

		std::array<BVec, 2> cmdWord = { 16_b, 16_b };
		Enum<HyperBusCommand::Type> cmdType;
		Enum<HyperBusCommand::Space> cmdSpace;

		ready(ctrl->command) = '0';
		IF(state.current() == State::idle)
		{
			ready(ctrl->command) = '1';
			csn = '1';

			IF(transfer(ctrl->command))
			{
				state = State::command;
				phy->dq.fen = '1';
				csn = '0';
			}
			cmdType = ctrl->command->type;
			cmdSpace = ctrl->command->space;

			cmdWord[0] = wordAddr(3, 16_b);
			cmdWord[1] = zext(wordAddr.lower(3_b), 16_b);
		}
		cmdWord = reg(cmdWord) | name("cmdWord");
		cmdType = reg(cmdType) | name("cmdType");
		cmdSpace = reg(cmdSpace) | name("cmdSpace");

		Bit extraLatency;
		IF(state.current() == State::command)
		{
			phy->dq.fen = '1';
			phy->dq.f = cmdWord[0];
			cmdWord[0] = cmdWord[1];
			extraLatency = phy->rwds.r->lsb();

			IF(ctr.value()(0, 1_b) == 1)
			{
				state = State::latency_initial;

				IF((cmdType == HyperBusCommand::write) & (cmdSpace == HyperBusCommand::reg))
					state = State::write; // zero latency write
			}
		}
		extraLatency = reg(extraLatency) | name("extraLatency");

		{	// latency wait states
			Enum<State> execState = State::read_latency;
			IF(cmdType == HyperBusCommand::write)
				execState = State::write;
			HCL_NAMED(execState);

			IF(state.current() == State::latency_initial)
			{
				IF(ctr.value() == ctrl->latency - 2)
				{
					state = execState;
					IF(extraLatency)
						state = State::latency_additional;
				}
			}

			IF(state.current() == State::latency_additional)
			{
				IF(ctr.value() == ctrl->latency - 1)
				{
					state = execState;
				}
			}
		}

		{ // write logic
			ready(ctrl->writeData) = '0';

			IF(state.current() == State::write)
			{
				ready(ctrl->writeData) = '1';
				sim_assert(transfer(ctrl->writeData)) << "no write data present in time";

				phy->dq.f = *ctrl->writeData;
				phy->dq.fen = '1';
				phy->rwds.f = ~byteEnable(ctrl->writeData);
				phy->rwds.fen = '1';

				IF(eop(ctrl->writeData))
					state = State::write_recover;
			}

			IF(state.current() == State::write_recover)
			{
				csn = '1';

				IF(ctr.value().lower(1_b) == 1)
					state = State::idle;
			}
		}

		{ // read logic
			eop(*ctrl->readData) = reg(ctrl->readStop) | name("readStopEnd");

			IF(state.current() == State::read_latency)
			{
				IF(ctr.value().lower(1_b) == 1)
					state = State::read;
			}

			IF(state.combinatorial() == State::read)
			{
				// we need to release csn one cycle ahead for phy
				IF(ctrl->readStop | eop(*ctrl->readData))
					csn = '1';
			}

			**ctrl->readData = *phy->dq.r;
			valid(*ctrl->readData) = '0';
			IF(state.current() == State::read)
			{
				valid(*ctrl->readData) = '1';
				IF(eop(*ctrl->readData))
					state = State::wait;
			}
		}

		IF(state.current() == State::wait)
		{
			csn = '1';
			state = State::idle;
		}

		HCL_NAMED(ctrl);
		return ctrl;
	}

	scl::TileLinkU hyperBusTileLink(Reverse<HyperBusController> controller, HyperBusTileLinkConfig cfg)
	{
		Area ent{ "scl_hyperBusTileLink", true };
		HCL_NAMED(controller);

		scl::TileLinkU link = scl::tileLinkInit({
			.addrW = cfg.addrW,
			.dataW = controller->writeData->width(),
			.sourceW = cfg.sourceW,
			.burstLimitLog = utils::Log2C(cfg.burstLimit)
		});
		ready(link.a) = '0';
		valid(*link.d) = '0';

		enum class State
		{
			idle,	// wait for and transmit command to controller
			write,	// transfer write data
			read,	// transfer read data
		};
		Reg<Enum<State>> state{ State::idle };
		state.setName("state");

		TileLinkD response = constructFrom(**link.d);
		UInt readBurstLen = BitWidth::last(cfg.burstLimit);

		// setup command
		{
			valid(controller->command) = '0';

			controller->command->type = HyperBusCommand::write;
			IF(link.a->isGet())
				controller->command->type = HyperBusCommand::read;

			controller->command->space = HyperBusCommand::mem;
			IF(cfg.registerAccess)
				controller->command->space = HyperBusCommand::reg;

			controller->command->burst = HyperBusCommand::linear;
			controller->command->wordAddress = zext(link.a->wordAddress());
			controller->latency = cfg.latency;

			IF(state.current() == State::idle)
			{
				response = tileLinkDefaultResponse(*link.a);
				readBurstLen = resizeTo(transferLengthFromLogSize(link.a->size, link.a->mask.width().bits()), readBurstLen.width());

				valid(controller->command) = valid(link.a);
				IF(transfer(controller->command))
				{
					IF(link.a->isPut())
					{
						state = State::write;
					}
					ELSE
					{
						ready(link.a) = '1';
						state = State::read;
					}
				}
			}
		}
		response = reg(response) | name("response");
		readBurstLen = reg(readBurstLen) | name("readBurstLen");

		valid(*link.d) = '0';
		**link.d = response;

		// write path
		{
			valid(controller->writeData) = valid(link.a);
			*controller->writeData = link.a->data;
			byteEnable(controller->writeData) = link.a->mask;

			IF(state.current() == State::write)
			{
				ready(link.a) = ready(controller->writeData);
				IF(transfer(controller->writeData) & eop(controller->writeData))
				{
					valid(*link.d) = '1';
					sim_assert(transfer(*link.d)) << "response stream is not ready for hyper bus write ack";
					state = State::idle;
				}
			}

			// eop helper uses final ready
			eop(controller->writeData) = eop(link.a);
		}

		// read path
		{
			(*link.d)->data = **controller->readData;

			Counter burstStep(readBurstLen);
			controller->readStop = (burstStep.value() == readBurstLen - 1);

			IF(state.current() == State::read)
			{
				valid(*link.d) = valid(*controller->readData);
				sim_assert(!valid(*link.d) | transfer(*link.d)) << "response stream is not ready for hyper bus read data";

				IF(valid(*link.d))
				{
					burstStep.inc();
					IF(burstStep.isLast())
						state = State::idle;
				}
			}
		}

		HCL_NAMED(link);
		return link;
	}
}

