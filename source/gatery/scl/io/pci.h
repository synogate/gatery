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
			UInt bus = 8_b;
			UInt dev = 5_b;
			UInt func = 3_b;

			UInt ariFunc() const { return cat(dev, func); }
		};

		struct TlpHeaderAttr
		{
			UInt trafficClass = 3_b;
			Bit relaxedOrdering;
			Bit idBasedOrdering;
			Bit noSnoop;
		};

		struct MemTlpCplData
		{
			PciId requester;
			UInt tag = 8_b;
			TlpHeaderAttr attr;
			UInt lowerAddress = 7_b;

			void decode(const UInt& tlpHdr);
			void encode(UInt& tlpHdr) const;
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
			UInt prefix; // optional prefix
			UInt header; // 96b or 128b header
			UInt data; // payload

			Tlp discardHighAddressBits() const;
		};


		Bit isCompletionTlp(const UInt& tlpHeader);
		Bit isMemTlp(const UInt& tlpHeader);
		Bit isDataTlp(const UInt& tlpHeader);

		class AvmmBridge
		{
		public:
			AvmmBridge() = default;
			AvmmBridge(Stream<Packet<Tlp>>& rx, AvalonMM& avmm, const PciId& cplId);

			void setup(Stream<Packet<Tlp>>& rx, AvalonMM& avmm, const PciId& cplId);

			Stream<Packet<Tlp>>& tx() { return m_tx; }

		protected:
			void generateFifoBridge(Stream<Packet<Tlp>>& rx, AvalonMM& avmm);

		private:
			Stream<Packet<Tlp>> m_tx;
			PciId m_cplId;
		};

		struct IntelPTileCompleter
		{
			void generate();

			std::array<Stream<Packet<Tlp>>, 2> in;
			Stream<Packet<Tlp>> out;
			AvalonMM avmm;
			PciId completerId;
		};
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::PciId, bus, dev, func);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::TlpHeaderAttr, trafficClass, relaxedOrdering, idBasedOrdering, noSnoop);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::MemTlpCplData, requester, tag, attr, lowerAddress);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::Tlp, prefix, header, data);
