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
#include <gatery/scl/io/pci.h>

namespace gtry::scl::sim {

	struct DefaultBitVectorWriter {
		gtry::sim::DefaultBitVectorState &destination;
		size_t offset = 0;
		DefaultBitVectorWriter(gtry::sim::DefaultBitVectorState &destination) : destination(destination) {}

		template<typename T>
		DefaultBitVectorWriter& write(const T& value, BitWidth size) {
			HCL_DESIGNCHECK_HINT(offset + size.value <= destination.size(), "not enough space in destination to write value");
			destination.insert(gtry::sim::DefaultConfig::VALUE, offset, size.value, (gtry::sim::DefaultConfig::BaseType) value);
			destination.setRange(gtry::sim::DefaultConfig::DEFINED, offset, size.value);
			offset += size.value;
			return *this;
		}

		DefaultBitVectorWriter& skip(BitWidth size) {
			HCL_DESIGNCHECK_HINT(offset + size.value <= destination.size(), "offset would overflow after this operation. This is not allowed");
			offset += size.value;
			return *this;
		}
	};


	struct TlpInstruction {
		scl::pci::TlpOpcode opcode;
		bool	th = 0;						//presence of TPH (processing hint)
		bool	idBasedOrderingAttr2 = 0;	//Attribute[2]
		uint8_t	tc = 0b000;					//traffic class
		std::optional<size_t> length;		//length
		size_t at = 0b00;					//Address type
		bool	noSnoopAttr0 = 0;
		bool	relaxedOrderingAttr1 = 0;
		bool	ep = 0;							//indicator of poisoned tlp
		bool	td = 0;							//tlp digest
		uint8_t ph = 0b00;

		size_t requesterID = 0xABCD;
		uint8_t tag = 0xEF;
		size_t firstDWByteEnable = 0b1111;
		size_t lastDWByteEnable = 0b1111;
		std::optional<uint64_t> wordAddress;
		std::optional<uint8_t> lowerByteAddress;
		std::optional<size_t> completerID;
		size_t completionStatus = 0b000;
		bool byteCountModifier = 0;
		std::optional<size_t> byteCount;
		std::optional<std::vector<uint32_t>> payload;

		TlpInstruction& safeLength(size_t length) { this->length = length; HCL_DESIGNCHECK_HINT(!(*this->length == 1 && this->lastDWByteEnable != 0), "lastBE must be zero if length = 1"); return *this; }

		operator gtry::sim::DefaultBitVectorState();
		static TlpInstruction createFrom(const gtry::sim::DefaultBitVectorState& raw);
		static TlpInstruction randomize(pci::TlpOpcode op, size_t seed = 'p'+'i'+'z'+'z'+'a');
		auto operator <=> (const TlpInstruction& other) const = default;
	};

	std::ostream& operator << (std::ostream& s, const TlpInstruction& inst);
}
