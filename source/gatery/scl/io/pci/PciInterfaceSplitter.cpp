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
#include <gatery/scl_pch.h>
#include "PciInterfaceSplitter.h"
#include <gatery/scl/stream/StreamDemux.h>
#include <gatery/scl/stream/utils.h>

namespace gtry::scl::pci 
{
	TlpPacketStream<EmptyBits> interfaceSplitter(CompleterInterface&& compInt, RequesterInterface&& reqInt, TlpPacketStream<EmptyBits, BarInfo>&& rx)
	{
		interfaceSplitterRx(move(compInt.request), move(reqInt.completion), move(rx));
		return strm::arbitrate(move(compInt.completion), move(*reqInt.request));
	}

	void interfaceSplitterRx(TlpPacketStream<EmptyBits, BarInfo>&& completerRequest, TlpPacketStream<EmptyBits>&& requesterCompletion, TlpPacketStream<EmptyBits, BarInfo>&& rx)
	{
		Area area{ "scl_interfaceSplitterRx", true };

		Bit isCompletion = capture(
			HeaderCommon::fromRawDw0(*rx).isCompletion(),
			valid(rx) & sop(rx)
		);
		HCL_NAMED(isCompletion);

		StreamDemux<TlpPacketStream<EmptyBits, BarInfo>> rxDemux(move(rx), zext(isCompletion));
		completerRequest <<= rxDemux.out(0);
		HCL_NAMED(completerRequest);
		requesterCompletion <<= rxDemux.out(1).template remove<BarInfo>();
		HCL_NAMED(requesterCompletion);
	}
}
