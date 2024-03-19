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
#include <gatery/pch.h>
#include "PciInterfaceSplitter.h"
#include <gatery/scl/stream/StreamDemux.h>
#include <gatery/scl/stream/StreamArbiter.h>

namespace gtry::scl::pci {
	PciInterfaceSplitter::PciInterfaceSplitter(CompleterInterface& compInt, RequesterInterface& reqInt, TlpPacketStream<EmptyBits, BarInfo>&& rx)
	{
		this->rx(move(rx));
		auto localCompComp = constructFrom(compInt.completion);
		localCompComp <<= compInt.completion;
		this->completerCompletion(move(localCompComp));
		auto localReqReq = constructFrom(*reqInt.request);
		localReqReq <<= *reqInt.request;
		this->requesterRequest(move(localReqReq));
		this->generate();
		compInt.request = this->completerRequest();
		reqInt.completion = this->requesterCompletion();
	}
	
	void PciInterfaceSplitter::generate()
	{
		Area area{ "scl_pci_splitter", true};
		splitRx();
		mergeTx();
	}
	void PciInterfaceSplitter::splitRx()
	{
		HCL_DESIGNCHECK(m_rx);

		enum class RxSelector {
			selectCompleterRequest,
			selectRequesterCompletion
		};

		HeaderCommon hdr = capture(
			HeaderCommon::fromRawDw0((*m_rx)->lower(32_b)),
			HeaderCommon::makeDefault(TlpOpcode::completionWithData, 1),
			valid(*m_rx) & sop(*m_rx));

		HCL_NAMED(hdr);

		Enum<RxSelector> selector = RxSelector::selectRequesterCompletion;
		IF(hdr.is4dw())
			selector = RxSelector::selectCompleterRequest;
		HCL_NAMED(selector);
		scl::StreamDemux<TlpPacketStream<EmptyBits, BarInfo>> rxDemux(move(*m_rx), (UInt) selector.toBVec());
		m_completerRequest		= rxDemux.out((size_t) RxSelector::selectCompleterRequest);
		m_requesterCompletion	= rxDemux.out((size_t) RxSelector::selectRequesterCompletion).template remove<BarInfo>();
	}
	void PciInterfaceSplitter::mergeTx()
	{
		HCL_DESIGNCHECK(m_completerCompletion);
		HCL_DESIGNCHECK(m_requesterRequest);

		scl::StreamArbiter<TlpPacketStream<EmptyBits>> txArbiter;
		txArbiter.attach(*m_requesterRequest);
		txArbiter.attach(*m_completerCompletion);
		m_tx = move(txArbiter.out());
		txArbiter.generate();
	}
}
