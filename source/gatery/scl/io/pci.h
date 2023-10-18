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
#include <gatery/frontend.h>
#include "../Avalon.h"
#include "../stream/Packet.h"

namespace gtry::scl
{
	namespace pci
	{
		struct PciId
		{
			BVec bus = 8_b;
			BVec dev = 5_b;
			BVec func = 3_b;

			BVec ariFunc() const { return (BVec) cat(dev, func); }
		};

		struct TlpHeaderAttr
		{
			BVec trafficClass = 3_b;
			Bit relaxedOrdering;
			Bit idBasedOrdering;
			Bit noSnoop;
		};

		struct MemTlpCplData
		{
			PciId requester;
			BVec tag = 8_b;
			TlpHeaderAttr attr;
			UInt lowerAddress = 7_b;

			void decode(const BVec& tlpHdr);
			void encode(BVec& tlpHdr) const;
		};

		namespace TlpOffset
		{
			constexpr size_t spec_offset(size_t bit, size_t byte, size_t word = 0) { return word * 32 + (3 - byte) * 8 + bit; }

			constexpr size_t hasPrefix = spec_offset(7, 0, 0);
			constexpr size_t hasData = spec_offset(6, 0, 0);
			constexpr size_t has4DW = spec_offset(5, 0, 0);
			constexpr Selection type{ .start = (int)spec_offset(0, 0, 0), .width = 5 };

		}

		struct Tlp
		{
			BVec prefix; // optional prefix
			BVec header; // 96b or 128b header
			BVec data; // payload

			Tlp discardHighAddressBits() const;
		};


		Bit isCompletionTlp(const BVec& tlpHeader);
		Bit isMemTlp(const BVec& tlpHeader);
		Bit isDataTlp(const BVec& tlpHeader);

		class AvmmBridge
		{
		public:
			AvmmBridge() = default;
			AvmmBridge(RvStream<Tlp>& rx, AvalonMM& avmm, const PciId& cplId);

			void setup(RvStream<Tlp>& rx, AvalonMM& avmm, const PciId& cplId);

			RvStream<Tlp>& tx() { return m_tx; }

		protected:
			void generateFifoBridge(RvStream<Tlp>& rx, AvalonMM& avmm);

		private:
			RvStream<Tlp> m_tx;
			PciId m_cplId;
		};

		struct IntelPTileCompleter
		{
			void generate();

			std::array<RvPacketStream<Tlp>, 2> in;
			RvPacketStream<Tlp> out;
			AvalonMM avmm;
			PciId completerId;
		};
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::PciId, bus, dev, func);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::TlpHeaderAttr, trafficClass, relaxedOrdering, idBasedOrdering, noSnoop);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::MemTlpCplData, requester, tag, attr, lowerAddress);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::Tlp, prefix, header, data);
