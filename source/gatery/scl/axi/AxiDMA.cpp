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
#include "gatery/pch.h"
#include "AxiDMA.h"

namespace gtry::scl
{
	RvStream<AxiAddress> axiGenerateAddressFromCommand(RvStream<AxiToStreamCmd>&& cmd, const AxiConfig& config)
	{
		RvStream<AxiAddress> out;
		out->addr = config.addrW;
		out->id = ConstBVec(cmd->id ,config.idW);
		out->user = ConstBVec(config.arUserW);

		HCL_DESIGNCHECK_HINT(cmd->bytesPerBurst >= config.dataW.bytes(), "Burst size must be at least as large as the data width of the AXI interface.");
		out->size = utils::Log2C(config.dataW.bytes());

		HCL_DESIGNCHECK_HINT(cmd->bytesPerBurst % config.dataW.bytes() == 0, "Burst size must be a multiple of the data width of the AXI interface.");
		out->len = cmd->bytesPerBurst / config.dataW.bytes();

		out->burst = (size_t)AxiBurstType::INCR;
		out->cache = 0;
		out->prot = 0;
		out->qos = 0;
		out->region = 0;

		valid(out) = valid(cmd);

		// address calculation logic. 
		// first out beat is in IDLE state with cmd->startAddress followed by incremental address register beats.
		enum State { IDLE, TRANSFER };
		Reg<Enum<State>> state{ IDLE };
		UInt address = cmd->startAddress.width();

		IF(state.current() == IDLE)
		{
			address = cmd->startAddress;
			IF(transfer(out))
				state = TRANSFER;
		}

		out->addr = zext(address);
		IF(transfer(out))
			address += cmd->bytesPerBurst;

		ready(cmd) = '0';
		IF(state.current() == TRANSFER & address == cmd->endAddress)
		{
			ready(cmd) = '1';
			state = IDLE;
		}
		address = reg(address);

		return out;
	}

	RvPacketStream<BVec> axiToStream(RvPacketStream<AxiReadData>&& dataStream)
	{
		return dataStream.transform(&AxiReadData::data);
	}

	RvPacketStream<BVec> axiToStream(RvStream<AxiToStreamCmd>&& cmd, Axi4& axi)
	{
		Area area{ "scl_axiToStream", true };
		HCL_NAMED(axi);
		HCL_NAMED(cmd);

		*axi.ar <<= axiGenerateAddressFromCommand(move(cmd), axi.config());
		Stream out = axiToStream(move(axi.r));
		HCL_NAMED(out);
		return out;
	}
}
