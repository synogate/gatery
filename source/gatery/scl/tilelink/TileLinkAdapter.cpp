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
#include "tilelink.h"
#include "TileLinkMasterModel.h"
#include "TileLinkAdapter.h"

namespace gtry::scl
{
	scl::TileLinkUL tileLinkHalfWidth(scl::TileLinkUL&& slave)
	{
		Area ent{ "scl_tileLinkHalfWidth", true };
		HCL_DESIGNCHECK_HINT(slave.a->source.width() >= 1_b, "tileLinkHalfWidth requires 1 source bit.");

		scl::TileLinkUL master = tileLinkInit<scl::TileLinkUL>(
			slave.a->address.width(),
			slave.a->data.width() / 2,
			slave.a->source.width() - 1_b
		);

		// request
		{
			size_t partSelBit = utils::Log2C(master.a->data.width().bytes());
			UInt partSel = master.a->address(partSelBit, 1_b);
			HCL_NAMED(partSel);

			ready(master.a) = ready(slave.a);
			valid(slave.a) = valid(master.a);
			slave.a->opcode = master.a->opcode;
			slave.a->param = master.a->param;
			slave.a->size = zext(master.a->size);
			slave.a->source = cat(partSel, master.a->source);
			slave.a->address = master.a->address;
			slave.a->data = (BVec)pack(master.a->data, master.a->data);
			slave.a->mask = 0;
			slave.a->mask.part(2, partSel) = master.a->mask;
		}
		
		// response
		{
			TileLinkChannelD& slaveD = *slave.d;
			TileLinkChannelD& masterD = *master.d;
			UInt partSelD = slaveD->source.upper(1_b);
			HCL_NAMED(partSelD);

			ready(slaveD) = ready(masterD);
			valid(masterD) = valid(slaveD);
			masterD->opcode = slaveD->opcode;
			masterD->param = slaveD->param;
			masterD->size = slaveD->size;
			masterD->source = slaveD->source.lower(-1_b);
			masterD->sink = slaveD->sink;
			masterD->data = slaveD->data.part(2, partSelD);
			masterD->error = slaveD->error;
		}

		master.addrSpaceDesc = slave.addrSpaceDesc;
		HCL_NAMED(master);		
		return master;
	}

	scl::TileLinkUB tileLinkDoubleWidth(scl::TileLinkUB& slave)
	{
		Area ent{ "scl_tileLinkDoubleWidth", true };
		HCL_NAMED(slave);

		scl::TileLinkUB master;
		scl::tileLinkInit(master,
			slave.a->address.width(),
			slave.a->data.width() * 2,
			slave.a->size.width(),
			slave.a->source.width()
		);

		scl::TileLinkChannelA masterA = strm::regReady(move(master.a));
		HCL_NAMED(masterA);

		// request
		valid(slave.a) = valid(masterA);
		slave.a->opcode = masterA->opcode;
		slave.a->param = masterA->param;
		slave.a->size = masterA->size;
		slave.a->source = masterA->source;
		slave.a->address = masterA->address;

		{
			Bit sendUpperHalfReg;
			sendUpperHalfReg = reg(sendUpperHalfReg, '0');
			HCL_NAMED(sendUpperHalfReg);

			// select word based on address for single beat requests
			Bit sendUpperHalf = sendUpperHalfReg;
			IF(valid(masterA) & !slave.a->isBurst())
			{
				size_t logBurstSize = utils::Log2(slave.a->mask.width().bits());
				sendUpperHalf = masterA->address[logBurstSize];
			}

			HCL_NAMED(sendUpperHalf);
			slave.a->mask = muxWord(sendUpperHalf, masterA->mask);
			slave.a->data = muxWord(sendUpperHalf, masterA->data);

			// toggle between words for burst requests
			IF(transfer(slave.a) & slave.a->isBurst())
				sendUpperHalfReg = !sendUpperHalfReg;

			ready(masterA) = ready(slave.a) & (!slave.a->isBurst() | sendUpperHalfReg == '0');
		}


		// response
		scl::TileLinkD& sd = **slave.d;
		scl::TileLinkD& md = **master.d;

		md.opcode = sd.opcode;
		md.param = sd.param;
		md.size = sd.size;
		md.source = sd.source;
		md.sink = sd.sink;
		md.data = (BVec)cat(sd.data, sd.data);
		md.error = sd.error;

		{
			BVec lowWord = sd.data.width();
			IF(transfer(*slave.d))
				lowWord = sd.data;
			lowWord = reg(lowWord);

			IF(sd.isBurst())
				md.data = (BVec)cat(sd.data, lowWord);

			Bit secondBeatOfBurst;
			IF(transfer(*slave.d) & sd.isBurst())
				secondBeatOfBurst = !secondBeatOfBurst;
			secondBeatOfBurst = reg(secondBeatOfBurst, '0');

			ready(*slave.d) = ready(*master.d) | (!secondBeatOfBurst & sd.isBurst());
			valid(*master.d) = valid(*slave.d) & (secondBeatOfBurst | !sd.isBurst());
		}

		master.addrSpaceDesc = slave.addrSpaceDesc;
		HCL_NAMED(master);
		return master;
	}

	namespace internal
	{
		struct AddBurstSource
		{
			UInt source;
			UInt size;
			UInt sequence;
			Bit forward;
			Bit last;
		};

		static BitWidth addBurstSourceWidth(const TileLinkA& slave, BitWidth masterSizeW)
		{
			const AddBurstSource meta{
				.source = 0_b,
				.size = masterSizeW,
				.sequence = masterSizeW - utils::Log2(slave.mask.width().bits()),
			};
			BitWidth availableBits = slave.source.width();
			BitWidth requiredBits = width(meta);
			HCL_DESIGNCHECK_HINT(availableBits >= requiredBits, "more source bits required for adding burst support to tilelink slave");
			return availableBits - requiredBits;
		}

		static AddBurstSource addBurstRequest(TileLinkChannelA& slave, TileLinkChannelA& master)
		{
			slave->opcode = master->opcode;
			slave->param = master->param;
			slave->address = master->address;
			slave->mask = master->mask;
			slave->data = master->data;

			auto [sop, eop] = scl::seop(master);

			// limit burst size to one slave beat
			slave->size = master->size.lower(slave->size.width());
			size_t slaveBurstLimit = utils::Log2(slave->mask.width().bits());
			IF(master->size > slaveBurstLimit)
				slave->size = slaveBurstLimit;

			// generate address low bits for bursts
			UInt addressOffset = BitWidth{ master->size.width().last() };
			addressOffset = reg(addressOffset, 0);
			HCL_NAMED(addressOffset);

			slave->address |= zext(addressOffset);
			IF(transfer(slave))
			{
				addressOffset += slave->mask.width().bits();
				IF(transfer(master) & eop.eop)
					addressOffset = 0;
			}

			// generate instructions for response circuit
			AddBurstSource source{
				.source = master->source,
				.size = master->size,
				.sequence = master->size.width() - slaveBurstLimit,
				.forward = '1'
			};

			// sequence is a counter to make all requests unique
			IF(transfer(slave))
				source.sequence += 1;
			source.sequence = reg(source.sequence, 0);

			// forward instructs the response side to set valid high
			IF(master->isPut() & !eop.eop)
				source.forward = '0';

			// last is used to aggregate error signal on response side
			source.last = transfer(master) & eop.eop;

			HCL_NAMED(source);
			slave->source = pack(source);

			// hold back read burst requests until all sub requests are issued
			Bit lastBeat = '1';
			IF(!master->hasData() & valid(master))
			{
				UInt numBeats = transferLengthFromLogSize(master->size, slave->mask.width().bits());
				HCL_NAMED(numBeats);
				UInt currentBeat = numBeats.width();
				currentBeat = reg(currentBeat, 0);
				HCL_NAMED(currentBeat);

				IF(transfer(slave))
					currentBeat += 1;

				lastBeat = numBeats == currentBeat;
				HCL_NAMED(lastBeat);
				IF(transfer(slave) & lastBeat)
					currentBeat = 0;
			}

			ready(master) = ready(slave) & lastBeat;
			valid(slave) = valid(master);
			return source;
		}

		static void addBurstResponse(TileLinkChannelD& slave, TileLinkChannelD& master, AddBurstSource& metaBlueprint)
		{
			AddBurstSource meta = constructFrom(metaBlueprint);
			unpack(slave->source, meta);
			HCL_NAMED(meta);

			master->opcode = slave->opcode;
			master->param = slave->param;
			master->size = meta.size;
			master->source = meta.source;
			master->sink = slave->sink;
			master->data = slave->data;

			// aggregate error over all responses of a single burst
			IF(valid(slave) & meta.last)
				master->error = '0';
			master->error = reg(master->error, '0');
			IF(valid(slave))
				master->error |= slave->error;

			// check in order property
			// this is not a TileLink requirement but generally true for pipelined slaves
			UInt seqCheck = meta.sequence.width();
			seqCheck = reg(seqCheck, 0);
			IF(transfer(slave))
			{
				sim_assert(seqCheck == meta.sequence) << "slave is out of order";
				seqCheck += 1;
			}

			// hide write response beats which are part of a burst
			valid(master) = valid(slave) & meta.forward;
			ready(slave) = ready(master) | !meta.forward;
		}
	}

	scl::TileLinkUB tileLinkAddBurst(scl::TileLinkUL& slave, BitWidth sizeW)
	{
		Area ent{ "scl_tileLinkAddBurst", true };

		scl::TileLinkUB master;
		scl::tileLinkInit(master,
			slave.a->address.width(),
			slave.a->data.width(),
			sizeW,
			internal::addBurstSourceWidth(*slave.a, sizeW)
		);
		HCL_NAMED(master);
		HCL_NAMED(slave);

		// this register is required to prevent response before request situations on long bursts and low latency slaves
		TileLinkChannelA aReg = strm::regReady(move(master.a));

		auto&& metaBlueprint = internal::addBurstRequest(slave.a, aReg);
		internal::addBurstResponse(*slave.d, *master.d, metaBlueprint);

		master.addrSpaceDesc = slave.addrSpaceDesc;
		HCL_NAMED(master);
		HCL_NAMED(slave);
		return master;
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::internal::AddBurstSource, source, size, sequence, forward, last);
