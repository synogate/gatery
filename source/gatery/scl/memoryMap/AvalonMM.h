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
#include "MemoryMap.h"

#if 0
namespace gtry::scl
{

	/// @deprecated Use the stand alone functions to connect command streams.
	class AvalonMMSlave : public MemoryMap
	{
	public:
		AvalonMMSlave(BitWidth addrWidth, BitWidth dataWidth);

		virtual void ro(const UInt& value, RegDesc desc) override;
		virtual void ro(const Bit& value, RegDesc desc) override;
		virtual Bit rw(UInt& value, RegDesc desc) override;
		virtual Bit rw(Bit& value, RegDesc desc) override;

		virtual void enterScope(std::string scope) override;
		virtual void leaveScope() override;

		UInt address;
		Bit write;
		BVec writeData;
		BVec readData;

		std::vector<RegDesc> addressMap;

		protected:
			std::vector<std::string> scopeStack;
			void addRegDescChunk(const RegDesc &desc, size_t offset, size_t width);
	};

	void pinIn(AvalonMMSlave& avmm, std::string prefix);
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::AvalonMMSlave, address, write, writeData, readData);
#endif