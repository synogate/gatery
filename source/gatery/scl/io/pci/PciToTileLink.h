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

#include <gatery/scl/io/pci/pci.h>
#include <gatery/scl/tilelink/tilelink.h>

namespace gtry::scl::pci {

	struct TlpAnswerInfo {
		HeaderCommon common;
		BVec requesterID = 16_b;
		BVec tag = 8_b;
		UInt lowerByteAddress = 7_b;
		Bit error = '0';
		static TlpAnswerInfo fromRequest(RequestHeader reqHdr);
		void setErrorFromLimitations(RequestHeader reqHdr);
	};

	TlpPacketStream<EmptyBits, BarInfo> completerRequestToTileLinkA(TileLinkChannelA& a, BitWidth tlpStreamW);
	TlpPacketStream<EmptyBits> tileLinkDToCompleterCompletion(TileLinkChannelD&& d, BitWidth tlpStreamW);

	CompleterInterface makeTileLinkMaster(scl::TileLinkUL&& tl, BitWidth tlpW);

	TlpPacketStream<EmptyBits> tileLinkAToRequesterRequest(TileLinkChannelA&& a, std::optional<BitWidth> tlpW = {});
	TileLinkChannelD requesterCompletionToTileLinkD(TlpPacketStream<EmptyBits>&& rc, BitWidth byteAddressW, BitWidth dataW);
	TileLinkChannelD requesterCompletionToTileLinkDFullW(TlpPacketStream<EmptyBits>&& rc);
	TileLinkChannelD requesterCompletionToTileLinkDCheapBurst(TlpPacketStream<EmptyBits>&& rc, std::optional<BitWidth> sizeW = {});
	TileLinkUL makePciMaster(RequesterInterface&& reqInt, BitWidth byteAddressW, BitWidth dataW, BitWidth tagW);
	TileLinkUL makePciMasterFullW(RequesterInterface&& reqInt);
	TileLinkUB makePciMasterCheapBurst(RequesterInterface&& reqInt, std::optional<BVec> tag = {}, std::optional<BitWidth> sizeW = {}, BitWidth addressW = 64_b);
}
