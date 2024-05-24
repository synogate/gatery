/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include "TileLinkValidator.h"
#include <gatery/simulation/Simulator.h>

#include <sstream>

namespace gtry::scl
{
	template<StreamSignal T> 
	bool readyDefined(const T &stream) { return true; }

	template<StreamSignal T> requires (T::template has<Ready>())
	bool readyDefined(const T &stream) {  return simu(ready(stream)).allDefined(); }

	template<StreamSignal T>
	bool readyValue(const T& stream) { return true; }

	template<StreamSignal T> requires (T::template has<Ready>())
	bool readyValue(const T& stream) { return (bool)simu(ready(stream)); }

	template<StreamSignal T> 
	bool validDefined(const T &stream) { return true; }

	template<StreamSignal T> requires (T::template has<Valid>())
	bool validDefined(const T &stream) {  return simu(valid(stream)).allDefined(); }

	template<StreamSignal T>
	bool validValue(const T& stream) { return true; }

	template<StreamSignal T> requires (T::template has<Valid>())
	bool validValue(const T& stream) { return (bool)simu(valid(stream)); }

	template<StreamSignal T>
	bool transferValue(const T& stream) { return readyValue(stream) & validValue(stream); }

	template<StreamSignal T>
	SimProcess validateStreamValid(T &stream, const Clock &clk)
	{
		while (true) {
			if (!validDefined(stream)) {
				hlim::BaseNode *node = valid(stream).readPort().node;
				std::stringstream msg;
				msg << "Stream has undefined valid signal at " << nowNs() << " ns.";// Node from " << node->getStackTrace();
				sim::SimulationContext::current()->onAssert(node, msg.str());
			}
			else if (validValue(stream) && !readyDefined(stream))
			{
				hlim::BaseNode* node = ready(stream).readPort().node;
				std::stringstream msg;
				msg << "Stream has undefined ready signal while valid is high at " << nowNs() << " ns.";// Node from " << node->getStackTrace();
				sim::SimulationContext::current()->onAssert(node, msg.str());
			}
			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLink(TileLinkChannelA &channelA, TileLinkChannelD &channelD, const Clock &clk)
	{
		fork(validateStreamValid(channelA, clk));
		fork(validateStreamValid(channelD, clk));
		fork(validateTileLinkControlSignalsDefined(channelA, clk));
		fork(validateTileLinkControlSignalsDefined(channelD, clk));
		//fork(validateTileLinkSourceReuse(channelA, channelD, clk)); TODO fix burst
		fork(validateTileLinkResponseMatchesRequest(channelA, channelD, clk));
		fork(validateTileLinkAlignment(channelA, clk));
		fork(validateTileLinkMask(channelA, clk));
		fork(validateTileLinkBurst(channelA, clk));
		fork(validateTileLinkBurst(channelD, clk));
		
		co_return;
	}

	SimProcess validateTileLinkSourceReuse(TileLinkChannelA& channelA, TileLinkChannelD& channelD, const Clock& clk)
	{
		std::vector<bool> sourceIdInUse(channelA->source.width().count(), false);

		while (true)
		{
			auto sourceD = simu(channelD->source);
			if (transferValue(channelD) && sourceD.allDefined())
			{
				// TODO: burst support
				sourceIdInUse[sourceD.value()] = false;
			}

			auto sourceA = simu(channelA->source);
			if (transferValue(channelA) && sourceA.allDefined())
			{
				if (sourceIdInUse[sourceA.value()])
				{
					hlim::BaseNode* node = channelA->source.readPort().node;
					std::stringstream msg;
					msg << "TileLink 5.4 violated: Source ID is reused while inflight at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}
				sourceIdInUse[sourceA.value()] = true;
			}

			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLinkResponseMatchesRequest(TileLinkChannelA& channelA, TileLinkChannelD& channelD, const Clock& clk)
	{
		struct RequestData
		{
			TileLinkA::OpCode op = TileLinkA::Intent;
			size_t size = 0;
		};
		std::vector<RequestData> request(channelA->source.width().count());

		while (true)
		{
			auto sourceD = simu(channelD->source);
			if (validValue(channelD) && sourceD.allDefined())
			{
				RequestData& req = request[sourceD.value()];
				
				if (req.size != simu(channelD->size))
				{
					hlim::BaseNode* node = channelA->size.readPort().node;
					std::stringstream msg;
					msg << "TileLink violated: Request size must match response size at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}

				TileLinkD::OpCode res = (TileLinkD::OpCode)simu(channelD->opcode).value();
				switch (req.op)
				{
				case TileLinkA::Get:
					if (res != TileLinkD::AccessAckData)
					{
						hlim::BaseNode* node = channelA->size.readPort().node;
						std::stringstream msg;
						msg << "TileLink 6.1 violated: A response to Get must be AccessAckData at " << nowNs() << " ns.";
						sim::SimulationContext::current()->onAssert(node, msg.str());
					}
					break;
				case TileLinkA::PutFullData:
				case TileLinkA::PutPartialData:
					if (res != TileLinkD::AccessAck)
					{
						hlim::BaseNode* node = channelA->size.readPort().node;
						std::stringstream msg;
						msg << "TileLink 6.1 violated: A response to Put* must be AccessAck at " << nowNs() << " ns.";
						sim::SimulationContext::current()->onAssert(node, msg.str());
					}
					break;
				case TileLinkA::ArithmeticData:
				case TileLinkA::LogicalData:
					if (res != TileLinkD::AccessAckData)
					{
						hlim::BaseNode* node = channelA->size.readPort().node;
						std::stringstream msg;
						msg << "TileLink 7.1 violated: A response to atomic operations must be AccessAckData at " << nowNs() << " ns.";
						sim::SimulationContext::current()->onAssert(node, msg.str());
					}
					break;
				case TileLinkA::Intent:
					if (res != TileLinkD::HintAck)
					{
						hlim::BaseNode* node = channelA->size.readPort().node;
						std::stringstream msg;
						msg << "TileLink 7.1 violated: A response to Intent must be HintAck at " << nowNs() << " ns.";
						sim::SimulationContext::current()->onAssert(node, msg.str());
					}
					break;
				default: // silently ignore unknown opcodes
					break;
				}
			}

			auto sourceA = simu(channelA->source);
			if (transferValue(channelA) && sourceA.allDefined())
			{
				RequestData& req = request[sourceA.value()];
				req.op = (TileLinkA::OpCode)simu(channelA->opcode).value();
				req.size = simu(channelA->size);
			}

			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLinkControlSignalsDefined(TileLinkChannelA& a, const Clock& clk)
	{
		while (true)
		{
			if (simu(valid(a)))
			{
				auto check = [&](auto&& sig, std::string_view name) {
					if (!simu(sig).allDefined())
					{
						hlim::BaseNode* node = sig.readPort().node;
						std::stringstream msg;
						msg << "TileLink 4.1 violated: a_" << name << " undefined while valid is high at " << nowNs() << " ns.";
						sim::SimulationContext::current()->onAssert(node, msg.str());
					}
				};
				
				check(a->opcode, "opcode");
				check(a->param, "param");
				check(a->size, "size");
				check(a->source, "source");
				check(a->address, "address");
				check(a->mask, "mask");
				// a->data is allowed to be undefined
			}
			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLinkControlSignalsDefined(TileLinkChannelD& d, const Clock& clk)
	{
		while (true)
		{
			if (simu(valid(d)))
			{
				auto check = [&](auto&& sig, std::string_view name) {
					if (!simu(sig).allDefined())
					{
						hlim::BaseNode* node = sig.readPort().node;
						std::stringstream msg;
						msg << "TileLink 4.1 violated: d_" << name << " undefined while valid is high at " << nowNs() << " ns.";
						sim::SimulationContext::current()->onAssert(node, msg.str());
					}
				};

				check(d->opcode, "opcode");
				check(d->param, "param");
				check(d->size, "size");
				check(d->source, "source");
				check(d->sink, "sink");
				// d->data is allowed to be undefined
				check(d->error, "error");
			}
			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLinkNoBurst(TileLinkChannelA& a, const Clock& clk)
	{
		const size_t sizeLimit = utils::Log2C(a->mask.width().bits());

		while (true)
		{
			if (simu(valid(a)))
			{
				if (simu(a->size) > sizeLimit)
				{
					hlim::BaseNode* node = a->size.readPort().node;
					std::stringstream msg;
					msg << "TileLink 6 TL-UL violated: Burst is not allowed at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}
			}
			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLinkAlignment(TileLinkChannelA& a, const Clock& clk)
	{
		while (true)
		{
			if (simu(valid(a)))
			{
				const size_t mask = utils::bitMaskRange(0, simu(a->size).value());
				if ((simu(a->address).value() & mask) != 0)
				{
					hlim::BaseNode* node = a->size.readPort().node;
					std::stringstream msg;
					msg << "TileLink 4.6 violated: Address must be aligned to access size at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}
			}
			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLinkOperations(TileLinkChannelA& a, std::vector<TileLinkA::OpCode> whitelist, const Clock& clk)
	{
		while (true)
		{
			if (simu(valid(a)))
			{
				TileLinkA::OpCode op = (TileLinkA::OpCode)simu(a->opcode).value();
				if (find(whitelist.begin(), whitelist.end(), op) == whitelist.end())
				{
					hlim::BaseNode* node = a->opcode.readPort().node;
					std::stringstream msg;
					msg << "TileLink violated: a_opcode is not allowed by TileLink conformance level at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}
			}
			co_await OnClk(clk);
		}
	}

	SimProcess validateTileLinkMask(TileLinkChannelA& a, const Clock& clk)
	{
		while (true)
		{
			if (simu(valid(a)))
			{
				const size_t bytePerBeat = a->mask.width().bits();
				const size_t byteSize = 1ull << simu(a->size).value();
				const size_t byteOffset = simu(a->address).value() & (bytePerBeat - 1);
				const size_t byteMask = utils::bitMaskRange(byteOffset, std::min(byteSize, bytePerBeat));

				uint64_t mask = simu(a->mask).value();
				if((~byteMask & mask) != 0)
				{
					hlim::BaseNode* node = a->mask.readPort().node;
					std::stringstream msg;
					msg << "TileLink 4.6 violated: a_mask must be LOW for all inactive byte lanes at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}

				TileLinkA::OpCode op = (TileLinkA::OpCode)simu(a->opcode).value();
				if (op != TileLinkA::PutPartialData && byteMask != mask)
				{
					hlim::BaseNode* node = a->mask.readPort().node;
					std::stringstream msg;
					msg << "TileLink 4.6 violated: The bits of a_mask must be HIGH for all active byte lanes at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}

			}
			co_await OnClk(clk);
		}
	}

	static size_t tileLinkBeats(const auto& chan)
	{
		const size_t bytePerBeat = (chan->data.width() / 8).bits();
		size_t byteSize = 1ull << simu(chan->size).value();
		return (byteSize + bytePerBeat - 1) / bytePerBeat;
	}

	SimProcess validateTileLinkBurst(TileLinkChannelA& a, const Clock& clk)
	{
		while (true)
		{
			size_t burstBeats = 0;

			// wait for burst
			while (true)
			{
				if (transferValue(a))
				{
					TileLinkA::OpCode op = (TileLinkA::OpCode)simu(a->opcode).value();
					if (op == TileLinkA::PutFullData || op == TileLinkA::PutPartialData ||
						op == TileLinkA::ArithmeticData || op == TileLinkA::LogicalData)
					{
						burstBeats = tileLinkBeats(a);
						if (burstBeats > 1)
							break; // burst detected
					}
				}
				co_await OnClk(clk);
			}

			// capture control signals of first burst
			const size_t opcode = simu(a->opcode);
			const size_t param = simu(a->param);
			const size_t size = simu(a->size);
			const size_t source = simu(a->source);
			const size_t address = simu(a->address);

			for (size_t i = 0; i < burstBeats; ++i)
			{
				bool missmatch = false;
				missmatch |= opcode != simu(a->opcode);
				missmatch |= param != simu(a->param);
				missmatch |= size != simu(a->size);
				missmatch |= source != simu(a->source);
				missmatch |= address != simu(a->address);

				if (missmatch)
				{
					hlim::BaseNode* node = a->opcode.readPort().node;
					std::stringstream msg;
					msg << "TileLink 4.1 violated: Control signals must be stable during all beats of a burst at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}
				
				do 
					co_await OnClk(clk);
				while (!transferValue(a));
			}
		}
	}

	SimProcess validateTileLinkBurst(TileLinkChannelD& d, const Clock& clk)
	{
		while (true)
		{
			size_t burstBeats = 0;

			// wait for burst
			while (true)
			{
				if (transferValue(d))
				{
					TileLinkD::OpCode op = (TileLinkD::OpCode)simu(d->opcode).value();
					if (op == TileLinkD::AccessAckData || op == TileLinkD::GrantData)
					{
						burstBeats = tileLinkBeats(d);
						if (burstBeats > 1)
							break; // burst detected
					}
				}
				co_await OnClk(clk);
			}

			// capture control signals of first burst
			const size_t opcode = simu(d->opcode);
			const size_t param = simu(d->param);
			const size_t size = simu(d->size);
			const size_t source = simu(d->source);
			const size_t sink = simu(d->sink);
			const char error = simu(d->error);

			for (size_t i = 0; i < burstBeats; ++i)
			{
				bool missmatch = false;
				missmatch |= opcode != simu(d->opcode);
				missmatch |= param != simu(d->param);
				missmatch |= size != simu(d->size);
				missmatch |= source != simu(d->source);
				missmatch |= sink != simu(d->sink);
				missmatch |= error != simu(d->error) && i == burstBeats - 1;

				if (missmatch)
				{
					hlim::BaseNode* node = d->opcode.readPort().node;
					std::stringstream msg;
					msg << "TileLink 4.1 violated: Control signals must be stable during all beats of a burst at " << nowNs() << " ns.";
					sim::SimulationContext::current()->onAssert(node, msg.str());
				}

				do
					co_await OnClk(clk);
				while (!transferValue(d));
			}
		}
	}
}
