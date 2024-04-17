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

#include "gatery/scl_pch.h"

#include "TileLinkMemoryMap.h"

#include "PackedMemoryMap.h"
#include "../tilelink/tilelink.h"

#include <gatery/utils/Range.h>

#include <functional>

namespace gtry::scl
{
	BVec byteMaskToBitMask(BVec byteMask) {
		BVec result = ConstBVec(byteMask.width() * 8);
		for (auto i : utils::Range(byteMask.size()))
			result.word(i, 8_b) = (BVec) sext(byteMask[i]);
		return result;
	}


	/// Turns a memory map into a TileLinkUL slave, allowing the registers to be accessed by the tilelink bus.
	Reverse<TileLinkUL> toTileLinkUL(PackedMemoryMap &memoryMap, BitWidth dataW, BitWidth sourceW)
	{
		Area area("MMtoTileLinkUL", true);

		memoryMap.packRegisters(dataW);
		auto& tree = memoryMap.getTree();
		BitWidth addrWidth = BitWidth::count(tree.physicalDescription->size / 8_b);

		Reverse<TileLinkUL> toMaster = { tileLinkInit<TileLinkUL>(addrWidth, dataW, sourceW) };
		HCL_NAMED(toMaster);
		toMaster->addrSpaceDesc = memoryMap.getTree().physicalDescription;

		TileLinkChannelD d0;
		*d0 = tileLinkDefaultResponse(*toMaster->a);
		valid(d0) = valid(toMaster->a);

		Bit anyWriteHappening = transfer(toMaster->a) & toMaster->a->isPut();
		HCL_NAMED(anyWriteHappening);

		UInt wordAddress = toMaster->a->address.upper(-BitWidth::count(dataW.bytes()));
		HCL_NAMED(wordAddress);


		BVec readData = ConstBVec(0, d0->data.width());
		d0->error = '1';

		std::function<void(PackedMemoryMap::Scope& scope, std::uint64_t offsetInBits)> processRegs;
		processRegs = [&](PackedMemoryMap::Scope& scope, std::uint64_t offsetInBits) {
			for (auto& r : scope.physicalRegisters) {
				if (r.readSignal)
					setName(*r.readSignal, r.description->name + "_read");

				Bit selected = wordAddress == (offsetInBits + r.offsetInBits) / dataW;
				setName(selected, r.description->name + "_selected");

				if (r.readSignal)
				{
					readData |= reg(mux(
						selected, { 
							ConstBVec(0, d0->data.width()), 
							zext(*r.readSignal, d0->data.width()) 
						}
					));
				}

				IF (selected) {
					d0->error = '0';

					if (r.writeSignal) {
						IF (anyWriteHappening) {
							for (auto byte : utils::Range((r.writeSignal->size()+7)/8)) {
								BitWidth size = std::min(8_b, r.writeSignal->width() - byte * 8_b);
								IF (toMaster->a->mask[byte])
									(*r.writeSignal)(byte * 8, size) = toMaster->a->data(byte * 8, size);
							}
							setName(*r.writeSignal, r.description->name + "_maskedWrite");
							*r.onWrite = '1';
						}
					}
				}
				if (r.writeSignal) {
					setName(*r.writeSignal, r.description->name + "_write");
					setName(*r.onWrite, r.description->name + "_writeSelect");
				}
			}
			for (auto &c : scope.subScopes)
				processRegs(c, offsetInBits + c.offsetInBits);
		};
		processRegs(tree, 0);

		TileLinkChannelD d = regDownstream(move(d0));
		d->data = readData;
		setName(d, "response");

		ready(toMaster->a) = ready(d);
		*toMaster->d <<= d;

		Reverse<TileLinkUL> out = { tileLinkInit<TileLinkUL>(addrWidth, dataW, sourceW) };
		HCL_NAMED(out);
		*toMaster <<= regDecouple(move(*out));
		return out;
	}
}
