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
#pragma once
#include "gatery/scl_pch.h"
#include "TileLinkInterconnect.h"
#include "../stream/StreamDemux.h"
#include <gatery/hlim/NodeGroup.h>

namespace gtry::scl
{
	TileLinkAddressDecoder& TileLinkAddressDecoder::operator()(uint64_t addressOffset, TileLinkU&& slave)
	{
		for (const Slave& o : m_slave)
			HCL_DESIGNCHECK(o.link.cfg.dataW == slave.cfg.dataW);

		Slave& s = m_slave.emplace_back();
		s.link = constructFrom(slave);
		slave <<= s.link;
		s.addressOffset = addressOffset;
		s.responsePriority = (uint32_t)m_slave.size();
		return *this;
	}

	TileLinkU TileLinkAddressDecoder::generate(std::string instanceName)
	{
		Area ent{ "scl_TileLinkAddressDecoder", true };
		if(!instanceName.empty())
			ent.getNodeGroup()->setInstanceName(instanceName);

		m_slave.sort([](const Slave& a, const Slave& b) {
			return a.link.cfg.addrW < b.link.cfg.addrW;
		});
		HCL_NAMED(m_slave);

		TileLinkCfg cfg{
			.addrW = 0_b,
			.dataW = 0_b,
			.sourceW = BitWidth{~0ull},
			.burstLimitLog = 0,
		};
		for (const Slave& s : m_slave)
		{
			cfg.addrW = std::max({ cfg.addrW, s.link.cfg.addrW, BitWidth::last(s.addressOffset) });
			cfg.dataW = s.link.cfg.dataW;
			cfg.sourceW = std::min({ cfg.sourceW, s.link.cfg.sourceW });
			cfg.burstLimitLog = std::max({ cfg.burstLimitLog, s.link.cfg.burstLimitLog });
			cfg.opValid |= s.link.cfg.opValid;
		}

		if(m_useErrorResponder)
			m_slave.emplace_back(Slave{
				.link = tileLinkErrorResponder(cfg) | &tileLinkRegDecouple,
				.addressOffset = 0,
			});

		TileLinkU master = tileLinkInit(cfg);
		ready(master.a) = '0';
		
		// handle request channel
		StreamRouter aRouter(move(master.a));
		aRouter.lockPackets(false); // lock is not required since put packets do not change address during put

		for (Slave& s : m_slave)
		{
			BitWidth slaveAddrW = s.link.cfg.addrW;
			size_t addrOffset = s.addressOffset >> slaveAddrW.bits();
			const UInt& addrRequest = master.a->address.upper(-slaveAddrW);
			Bit addrMath = (addrOffset == addrRequest);
			s.link.a <<= aRouter(addrMath)
				| strm::transform([&s](const TileLinkA& a) {
					return TileLinkA{
						.opcode = a.opcode,
						.param = a.param,
						.size = resizeTo(a.size, s.link.a->size.width()),
						.source = a.source,
						.address = a.address.lower(s.link.cfg.addrW),
						.mask = a.mask,
						.data = a.data,
					};
				});
		}

		// handle response channel
		StreamArbiter<TileLinkChannelD> arbiter;
		for (Slave& s : m_slave)
		{
			auto reduced = transform(move(*s.link.d), [&cfg](const TileLinkD& d) {
				return TileLinkD{
					.opcode = d.opcode,
					.param = d.param,
					.size = resizeTo(d.size, BitWidth::last(cfg.burstLimitLog)),
					.source = d.source.lower(cfg.sourceW),
					.sink = d.sink,
					.data = d.data,
					.error = d.error,
				};
			});
			arbiter.attach(move(reduced), s.responsePriority);
		}

		*master.d <<= transform(move(arbiter.out()), [&](const TileLinkD& d) {
			return TileLinkD{
				.opcode = d.opcode,
				.param = d.param,
				.size = d.size.lower((*master.d)->size.width()),
				.source = d.source,
				.sink = d.sink,
				.data = d.data,
				.error = d.error,
			};
		});
		arbiter.generate();

		HCL_NAMED(master);
		return master;
	}

	TileLinkU tileLinkErrorResponder(const TileLinkCfg& cfg)
	{
		Area ent{ "scl_tileLinkErrorResponder", true };

		TileLinkU link = tileLinkInit(cfg);
		HCL_NAMED(link);

		if (!cfg.burstSupport())
		{
			*link.d <<= strm::transform(move(link.a), [](const TileLinkA& a) {
				TileLinkD d = tileLinkDefaultResponse(a);
				d.error = '1';
				return d;
				});
		}
		else
		{
			Bit eopA = eop(link.a);
			Bit eopD = eop(*link.d);

			valid(*link.d) = valid(link.a) & eopA;
			ready(link.a) = ready(*link.d) & (eopD | !eopA);

			**link.d = tileLinkDefaultResponse(*link.a);
			(**link.d).error = eopD;
		}

		return link;
	}

	TileLinkArbiter::TileLinkArbiter(TileLinkU&& slave, BitWidth arbiterSourceW) :
		m_arbiterSourceW(arbiterSourceW),
		m_cfg(slave.cfg),
		m_addressSpace(slave.addrSpaceDesc),
		m_handlerD(move(*slave.d))
	{
		HCL_DESIGNCHECK(m_cfg.sourceW >= arbiterSourceW);

		m_handlerA.out(slave.a);
		slave.a <<= m_handlerA.out();
		m_cfg.sourceW -= arbiterSourceW;

		m_ent.leave();
	}

	TileLinkArbiter::operator TileLinkU()
	{
		auto scope = m_ent.enter();
		HCL_DESIGNCHECK_HINT(m_portIndex < m_arbiterSourceW.count(), "too many master ports for arbiter source width");

		TileLinkU out = tileLinkInit(m_cfg);
		out.addrSpaceDesc = m_addressSpace;

		TileLinkChannelA expandedA = move(out.a) | strm::transform([this](const TileLinkA& a) { 
			return TileLinkA{
				.opcode = a.opcode,
				.param = a.param,
				.size = a.size,
				.source = cat(ConstUInt(m_portIndex, m_arbiterSourceW), a.source),
				.address = a.address,
				.mask = a.mask,
				.data = a.data,
			};
		});
		m_handlerA.attach(move(expandedA));

		Bit enabled;
		TileLinkChannelD expandedD = m_handlerD(enabled);
		enabled = expandedD->source.upper(m_arbiterSourceW) == m_portIndex;

		*out.d <<= transform(move(expandedD), [this](const TileLinkD& d) {
			return TileLinkD{
				.opcode = d.opcode,
				.param = d.param,
				.size = resizeTo(d.size, BitWidth::last(m_cfg.burstLimitLog)),
				.source = d.source.lower(-m_arbiterSourceW),
				.sink = d.sink,
				.data = d.data,
				.error = d.error,
			};
		});

		setName(out, "out" + std::to_string(m_portIndex));
		m_portIndex++;
		return out;
	}

	void TileLinkArbiter::generate()
	{
		m_handlerA.generate();
	}


}
