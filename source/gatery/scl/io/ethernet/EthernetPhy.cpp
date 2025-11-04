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
#include "EthernetPhy.h"
#include "../../crc.h"
#include "../../stream/strm.h"

#include <gatery/scl/arch/colognechip/io.h>
#include <gatery/scl/arch/colognechip/SpecialFunction.h>
#include <gatery/scl/memoryMap/MemoryMap.h>
#include <gatery/scl/memoryMap/MemoryMapConnectors.h>

#include <gatery/scl/stream/SimuHelpers.h>

namespace gtry::scl::eth
{
	Reverse<TxRxStream> phy(RMII& in)
	{
		Area ent{ "scl_rmii_phy", true };

		// rx side
		RvStream<BVec> rx0{ *in.rxd };
		valid(rx0) = reg(valid(rx0), '0');
		IF(*in.crs_dv & (*in.rxd != 0))
			valid(rx0) = '1';
		IF(!(*in.crs_dv | reg(*in.crs_dv, '0'))) // RMII spec changed to toggle DV for the last byte to indicate last byte
			valid(rx0) = '0';
		HCL_NAMED(rx0);

		// filter last beat
		RvStream<BVec> rx1 = regDownstream(move(rx0));
		valid(rx1) &= !edgeFalling(valid(rx0));

		TxRxStream out = { {2_b}, move(rx1) };
		
		// tx side
		ready(out.tx) = '1';
		in.tx_en = reg(valid(out.tx), '0');
		in.txd = reg(*out.tx & valid(out.tx));

		HCL_NAMED(out);
		return out;
	}

	RvStream<BVec> preambleFilter(RvStream<BVec> in)
	{
		Area ent{ "scl_preambleFilter", true };
		HCL_DESIGNCHECK_HINT(in->width() == 2_b, "not implemented");
		HCL_NAMED(in);

		enum class State
		{
			carrier, preamble, data
		};
		Reg<Enum<State>> state{ State::carrier };

		RvStream<BVec> out;
		out <<= in;

		IF(state.current() == State::carrier)
		{
			IF(valid(in) & *in == 1)
				state = State::preamble;
		}
		IF(state.current() == State::preamble)
		{
			IF(valid(in) & *in == 3)
				state = State::data;
		}
		IF(!valid(in))
			state = State::carrier;

		valid(out) &= state.current() == State::data;
		HCL_NAMED(out);
		return out;
	}

	RvStream<BVec> preambleInsert(RvStream<BVec> in)
	{
		Area ent{ "scl_preambleInsert", true };
		HCL_DESIGNCHECK_HINT(in->width() == 2_b, "not implemented");
		HCL_NAMED(in);

		enum class State
		{
			wait, preamble, data
		};
		Reg<Enum<State>> state{ State::wait };

		RvStream<BVec> out;
		out <<= in;

		IF(state.current() == State::wait)
		{
			valid(out) = '0';
			ready(in) = '0';
			IF(valid(in))
				state = State::preamble;
		}

		IF(state.current() == State::preamble)
		{
			ready(in) = '0';

			*out = 1;

			// 1 byte replaced by J/K in phy, 7 byte preamble, 1 byte SOF
			IF(scl::Counter(9*4).isLast())
			{
				*out = 3;
				state = State::data;
			}
		}

		IF(state.current() == State::data)
		{
			IF(!valid(in))
				state = State::wait;
		}

		HCL_NAMED(out);
		return out;
	}

	RvPacketStream<BVec, Error> tagInsert(RvPacketStream<BVec, Error> in, BVec tag)
	{
		Area ent{ "scl_tagInsert", true };
		HCL_NAMED(in);

		Counter idleCounter(4);
		IF(!idleCounter.isLast())
			idleCounter.inc();
		IF(valid(in) & eop(in))
			idleCounter.reset();

		Stream out = constructFrom(in);
		out <<= in;

		// due to defined inter packet gaps, we can just insert the next packets tag
		IF(idleCounter.value() == 2)
		{
			valid(out) = '1';
			eop(out) = '0';
			*out = tag;
		}

		HCL_NAMED(out);
		return out;
	}

	RvPacketStream<BVec> tagRemove(RvPacketStream<BVec> in)
	{
		Area ent{ "scl_tagRemove", true };

		Stream out = constructFrom(in);
		out <<= in;

		Bit keepNextBeat;
		valid(out) &= keepNextBeat;
		ready(in) |= !keepNextBeat;
		keepNextBeat = flag(transfer(in), transfer(in) & eop(in)) | name("keepNextBeat");

		HCL_NAMED(out);
		return out;
	}

	RvPacketStream<BVec> eopAdd(RvStream<BVec> in)
	{
		Area ent{ "scl_eopAdd", true };
		return move(in) | 
			strm::regDownstream() | 
			strm::attach(Eop{edgeFalling(valid(in))}) |
			name("out")
			;
	}

	RvStream<BVec> eopRemove(RvPacketStream<BVec> in)
	{
		Area ent{ "scl_eopRemove", true };
		return move(in) |
			strm::stall(reg(transfer(in) & eop(in), '0') | name("stall")) |
			strm::remove<Eop>() |
			name("out");
	}

	RvPacketStream<BVec, Error> crcCheck(RvPacketStream<BVec> in)
	{
		Area ent{ "scl_crcCheck", true };
		CrcState crc{ CrcParams::init(CrcWellKnownParams::CRC_32), 32_b };
		crc.remainder = reg(crc.remainder, crc.params.initialRemainder);

		Stream out = strm::attach(move(in), Error{ '0' });

		IF(transfer(out))
			crc.update((UInt)*out);

		error(out) = crc.checksum() != 0x2144DF1C;

		IF(transfer(out) & eop(out))
			crc.init();

		HCL_NAMED(out);
		return out;
	}

	void tinyNIC(MemoryMap& mmap, const Clock& refClk50)
	{
		Area ent{ "scl_tinyNIC", true };
		HCL_DESIGNCHECK(refClk50.absoluteFrequency() == 50_mhz);
		const BitWidth cpuW = 32_b;
		mmap.enterScope("eth");

		TransactionalFifo rxBuffer(2048 / cpuW.bytes(), PacketStream<BVec>{cpuW});
		TransactionalFifo txBuffer(2048 / cpuW.bytes(), PacketStream<BVec>{cpuW});

		{
			ClockScope refScope(refClk50);

			arch::colognechip::CC_ODDR clkddr;
			clkddr.D0() = '0';
			clkddr.D1() = '1';
			pinOut(clkddr.Q(), "rmii_ref_clk");

			scl::eth::RMII rmii;
			pinOut(rmii, "rmii");
			pinOut(Bit{ '0' }, "rmii_rst");
			pinOut(Bit{ '0' }, "rmii_mdc");
			pinOut(Bit{ '0' }, "rmii_mdio");

			scl::TxRxStream phy = *eth::phy(rmii);

			std::shared_ptr simrx = std::make_shared<RvStream<BVec>>(RvStream<BVec>{ 2_b });
			#
			DesignScope::get()->getCircuit().addSimulationProcess([=]() -> SimProcess {
				simu(valid(*simrx)) = '0';

				for(size_t i = 0; i < 3; ++i)
					co_await OnClk(refClk50);
				//co_await strm::sendPacket(*simrx, strm::SimPacket{ 0x4321, 16_b }, refClk50);
			});

			move(*phy.rx) |
				eth::preambleFilter |
				strm::simOverride(move(*simrx), "simrx") |
				eth::eopAdd |
				eth::crcCheck |
				strm::widthExtend(16_b) |
				strm::regDownstreamBlocking() |
				eth::tagInsert(BVec{ "16b0" }) |
				strm::widthExtend(cpuW) |
				strm::pushStoreForward<RvPacketStream<BVec>>(rxBuffer);

			phy.tx <<= 
				strm::pop(txBuffer) | 
				strm::widthReduce(16_b) |
				tagRemove |
				strm::widthReduce(2_b) |
				eth::eopRemove |
				strm::regDownstream() |
				eth::preambleInsert
				;
		}

		mapOut(mmap, rxBuffer, "rx");
		mapIn(mmap, txBuffer, "tx");

		rxBuffer.generate();
		txBuffer.generate();
		mmap.leaveScope();
	}
}

