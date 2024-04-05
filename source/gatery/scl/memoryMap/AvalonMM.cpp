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
#include "gatery/scl_pch.h"
#include "AvalonMM.h"

#if 0

namespace gtry::scl
{
	AvalonMMSlave::AvalonMMSlave(BitWidth addrWidth, BitWidth dataWidth) :
		address(addrWidth),
		writeData(dataWidth),
		readData(dataWidth)
	{
		write.resetValue('0');
		readData = 0;
	}

	void AvalonMMSlave::addRegDescChunk(const RegDesc &desc, size_t offset, size_t width)
	{
		RegDesc d = desc;
		d.name += std::to_string(offset / readData.size());
		d.desc += "Bitrange ";
		d.desc += std::to_string(offset);
		d.desc += " to ";
		d.desc += std::to_string(offset + width);
		d.usedRanges.clear();
		for (auto r : desc.usedRanges)
			if (r.offset > offset && r.offset < offset + readData.size()) {
				r.offset -= offset;
				r.size = std::min(r.size, readData.size());
				d.usedRanges.push_back(std::move(r));
			}
		addressMap.push_back(d);
	}

	void AvalonMMSlave::ro(const BVec& value, RegDesc desc)
	{
		desc.flags = F_READ;
		if (!scopeStack.empty()) desc.scope = scopeStack.back();
		for (size_t offset = 0; offset < value.size(); offset += readData.size())
		{
			const BitWidth width = std::min(readData.width(), value.width() - offset);

			IF(address == addressMap.size())
				readData = zext(value(offset, width));

			if (readData.size() < value.size())
				addRegDescChunk(desc, offset, width.bits());
			else
				addressMap.push_back(std::move(desc));
		}
	}

	Bit AvalonMMSlave::rw(BVec& value, RegDesc desc)
	{
		desc.flags = F_READ | F_WRITE;
		if (!scopeStack.empty()) desc.scope = scopeStack.back();
		Bit selected = '0';
		for (size_t offset = 0; offset < value.size(); offset += readData.size())
		{
			const BitWidth width = std::min(readData.width(), value.width() - offset);

			IF(address == addressMap.size())
			{
				readData = zext(value(offset, width));
				IF(write)
				{
					selected = '1';
					value(offset, width) = writeData(0, width);
				}
			}

			if (readData.size() < value.size())
				addRegDescChunk(desc, offset, width.bits());
			else
				addressMap.push_back(std::move(desc));
		}
		setName(value, desc.name);
		return selected;
	}

	void AvalonMMSlave::enterScope(std::string scope)
	{
		if (!scopeStack.empty())
			scope = scopeStack.back() + '.' + scope;
		scopeStack.push_back(scope);
	}
	void AvalonMMSlave::leaveScope()
	{
		scopeStack.pop_back();
	}

	void pinIn(AvalonMMSlave& avmm, std::string prefix)
	{
		avmm.address = gtry::pinIn(avmm.address.width()).setName(prefix + "_address");
		avmm.write = gtry::pinIn().setName(prefix + "_write");
		avmm.writeData = gtry::pinIn(avmm.writeData.width()).setName(prefix + "_write_data");
		gtry::pinOut(avmm.readData).setName(prefix + "_read_data");
	}

}

#endif