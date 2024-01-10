/*  This file is part of Gatery, a library for circuit design.
Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include <gatery/frontend.h>
#include <gatery/scl/io/pci/pci.h>
#include "XilinxPciStructs.h"

namespace gtry::scl::pci::xilinx {

	RequestHeader createHeader(const CompleterRequestDescriptor& desc, const CQUser& cqUser);
	CompleterCompletionDescriptor createDescriptor(const CompletionHeader& hdr);

	RequesterRequestDescriptor createDescriptor(const RequestHeader& hdr);
	CompletionHeader createHeader(const RequesterCompletionDescriptor& desc);
	
	
	/** @brief amd axi4 generic packet stream */
	template<Signal ...Meta>
	using Axi4PacketStream = RvPacketStream<BVec, DwordEnable, Meta...>;
	
	/**
	* @brief functions to convert between amd's proprietary tlp packet format to a Gatery Tlp Packet Stream
	* @toImprove:	add support for 3dw tlp creation, at the moment we generate only 4dw tlps, even when dealing with 32 bit
	*				requests, which is non-conformant with native tlps.
	*/
	TlpPacketStream<scl::EmptyBits, pci::BarInfo> completerRequestVendorUnlocking(Axi4PacketStream<CQUser>&& in);
	Axi4PacketStream<CCUser> completerCompletionVendorUnlocking(TlpPacketStream<EmptyBits>&& in);

	Axi4PacketStream<RQUser> requesterRequestVendorUnlocking(TlpPacketStream<scl::EmptyBits>&& in);
	TlpPacketStream<scl::EmptyBits> requesterCompletionVendorUnlocking(Axi4PacketStream<RCUser>&& in, bool straddle = false);

	RequesterInterface requesterVendorUnlocking(Axi4PacketStream<RCUser>&& completion, Axi4PacketStream<RQUser>& request);
	
}
	
