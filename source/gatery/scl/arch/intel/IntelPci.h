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

using namespace gtry::scl::pci;

namespace gtry::scl::arch::intel {

	/**
	 * @brief Big Endian representation of the header, using the pci-sig tlp encoding
	*/
	struct PTileHeader{
		BVec header = 128_b;
	};

	/**
	 * @brief Big Endian representation of the prefix, using the pci-sig tlp encoding
	*/
	struct PTilePrefix{
		BVec prefix = 32_b;
	};

	/**
	 * @brief 
	*/
	struct PTileBarRange {
		BVec encoding = 3_b;
	};


	TlpPacketStream<EmptyBits, PTileBarRange> ptileRxVendorUnlocking(scl::strm::RvPacketStream<BVec, scl::EmptyBits, PTileHeader, PTilePrefix, PTileBarRange>&& rx);

	template<Signal... MetaT>
	scl::strm::RvPacketStream<BVec, scl::Error, PTileHeader, PTilePrefix>&& ptileTxVendorUnlocking(TlpPacketStream<MetaT...>&& tx);

	//shoudl really not be intel specific, maybe pci.h
	//class PciInterfaceSeparator {
	//public:
	//	PciInterfaceSeparator();
	//
	//	template<Signal... MetaInT>
	//	PciInterfaceSeparator& attachRx(TlpPacketStream<MetaInT...>&& rx);
	//	
	//	template<Signal... MetaOutT>
	//	TlpPacketStream<MetaOutT...> getTx();
	//
	//	RequesterInterface& requesterInterface() { HCL_DESIGNCHECK(m_requesterInterface); return *m_requesterInterface; }
	//	CompleterInterface& completerInterface() { HCL_DESIGNCHECK(m_completerInterface); return *m_completerInterface; }
	//	
	//private:
	//	std::optional<RequesterInterface> m_requesterInterface;
	//	std::optional<CompleterInterface> m_completerInterface;
	//};
}


