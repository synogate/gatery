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
#include <gatery/scl_pch.h>
#include "IntelPci.h"
#include <gatery/scl/stream/strm.h>

using namespace gtry::scl::pci;

namespace gtry::scl::arch::intel {
	TlpPacketStream<EmptyBits, PTileBarRange> ptileRxVendorUnlocking(scl::strm::RvPacketStream<BVec, EmptyBits, PTileHeader, PTilePrefix, PTileBarRange>&& rx){
		Area area{ "ptile_rx_vendor_unlocking", true };
		BitWidth rxW = rx->width();
	
		//base stream is the stream comprised of the header. It is a one-beat packetStream.
		//we must construct it from scratch
		TlpPacketStream<EmptyBits, PTileBarRange> hdr(rxW);
		*hdr = zext(capture(swapEndian(get<PTileHeader>(rx).header), valid(rx) & sop(rx)));

		Bit hasAlreadyCapturedHdr = flag(valid(rx) & sop(rx), transfer(rx) & eop(rx), '0');//these two lines seem like overkill, there must be an easier logic
		valid(hdr) = flagInstantSet(valid(rx) & sop(rx) & !hasAlreadyCapturedHdr, ready(hdr), '0');
		eop(hdr) = '1';
		emptyBits(hdr) = rxW.bits() - 128; //4DW
		IF(valid(hdr) & HeaderCommon::fromRawDw0(hdr->lower(32_b)).is3dw())
			emptyBits(hdr) = rxW.bits() - 96; //3DW
		get<PTileBarRange>(hdr) = get<PTileBarRange>(rx);
	
		TlpPacketStream<EmptyBits, PTileBarRange> dataStrm = move(rx)
			| strm::remove<PTilePrefix>()
			| strm::remove<PTileHeader>();
			
		//must mask dataStrm if the header claims there is no data and signal ready upstream
		IF(transfer(hdr) & !HeaderCommon::fromRawDw0(hdr->lower(32_b)).hasData()) {
			valid(dataStrm) &= '0';
			ready(rx) = '1';
		}

		HCL_NAMED(hdr);
		HCL_NAMED(dataStrm);
	
		//insert the header into the data stream
		return strm::streamAppend(
			move(hdr),
			move(dataStrm)
			)
			| strm::regDownstream();
	}
	 

	scl::strm::RvPacketStream<BVec, EmptyBits, scl::Error, PTileHeader, PTilePrefix> ptileTxVendorUnlocking(TlpPacketStream<EmptyBits>&& tx){
		Area area{ "ptile_tx_vendor_unlocking", true };
		HCL_DESIGNCHECK_HINT(tx->width() >= 128_b, "the width needs to be larger than 4DW for this implementation");
		setName(tx, "tlp_tx");
		//add header as metaSignal
		BVec rawHdr = capture(tx->lower(128_b), valid(tx) & sop(tx));
		IF(pci::HeaderCommon::fromRawDw0(rawHdr.lower(32_b)).is3dw())
			rawHdr.upper(32_b) = 0;
		
		TlpPacketStream<EmptyBits, PTileHeader> localTx = attach(move(tx), PTileHeader{ swapEndian(rawHdr) });
		
		//remove header from front of TLP
		UInt headerSizeInBits = 128;
		IF(pci::HeaderCommon::fromRawDw0(rawHdr.lower(32_b)).is3dw())
			headerSizeInBits = 96;

		return (strm::streamShiftRight(move(localTx), headerSizeInBits)
			| strm::attach(Error{ '0' })
			| strm::attach(PTilePrefix{ BVec{"32d0"} })
			).template reduceTo<scl::strm::RvPacketStream<BVec, EmptyBits, scl::Error, PTileHeader, PTilePrefix>>();
	}
}


