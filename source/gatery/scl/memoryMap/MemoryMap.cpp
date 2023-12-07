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
#include "gatery/pch.h"
#include "MemoryMap.h"
#include <gatery/scl/driverUtils/MemoryMapEntry.h>

namespace gtry::scl
{
    std::size_t hash_value(AddressSpaceDescription const& d)
    {
		std::size_t hash = 31337;
		boost::hash_combine(hash, d.offsetInBits);
		boost::hash_combine(hash, d.size.bits());
		//boost::hash_combine(hash, d.flags);
		boost::hash_combine(hash, d.name);
		boost::hash_combine(hash, d.descShort);
		boost::hash_combine(hash, d.descLong);
		boost::hash_range(hash, d.children.begin(), d.children.end()); // this is probably a bad idea
        return hash;
    }

	void format(std::ostream &stream, const AddressSpaceDescription &desc, size_t indent)
	{
		std::cout << std::string(indent, ' ') << " From: " << (desc.offsetInBits / 8) << " size " << desc.size << std::endl;
		std::cout << std::string(indent, ' ') << "    Name: " << desc.name << std::endl;
		std::cout << std::string(indent, ' ') << "    Short desc: " << desc.descShort << std::endl;
		std::cout << std::string(indent, ' ') << "    Long desc: " << desc.descLong << std::endl;
		std::cout << std::string(indent, ' ') << "    Children: " << std::endl;
		for (const auto &c : desc.children)
			format(stream, c, indent + 8);
	}

	std::vector<MemoryMapEntry> exportAddressSpaceDescription(const AddressSpaceDescription &desc)
	{
		std::vector<MemoryMapEntry> result;

		std::map<const AddressSpaceDescription*, size_t> childrenStartBackpointers;

		std::list<const AddressSpaceDescription*> queue;
		queue.push_back(&desc);
		while (!queue.empty()) {
			auto *d = queue.front();
			queue.pop_front();

			auto it = childrenStartBackpointers.find(d);
			if (it != childrenStartBackpointers.end())
				result[it->second].childrenStart = (std::uint32_t) result.size();

			if (!d->children.empty())
				childrenStartBackpointers[&d->children.front().get()] = result.size();

			MemoryMapEntry entry = {
				.addr = d->offsetInBits,
				.width = d->size.value,
				.flags = 0,
				.name = d->name.get().c_str(),
				.shortDesc = d->descShort.get().c_str(),
				.longDesc = d->descLong.get().c_str(),
				.childrenStart = 0,
				.childrenCount = (std::uint32_t) d->children.size(),
			};

			result.push_back(entry);

			for (const auto &c : d->children)
				queue.push_back(&c.get());
		}

		return result;
	}

	void format(std::ostream &stream, std::string_view name, const std::vector<MemoryMapEntry> &memoryMap)
	{
		stream << "static constexpr MemoryMapEntry " << name << "[] = {" << std::endl;
		for (const auto &e : memoryMap)
			stream 
				<< "    MemoryMapEntry {" << std::endl
				<< "        .addr = " << e.addr << ",\n"
				<< "        .width = " << e.width << ",\n"
				<< "        .flags = " << (unsigned)e.flags << ",\n"
				<< "        .name = \"" << e.name << "\",\n"
				<< "        .shortDesc = \"" << e.shortDesc << "\",\n"
				<< "        .longDesc = \"" << e.longDesc << "\",\n"
				<< "        .childrenStart = " << e.childrenStart << ",\n"
				<< "        .childrenCount = " << e.childrenCount << ",\n"
				<< "    },\n";
		stream << "};" << std::endl;
	}


	void MemoryMap::SelectionHandle::joinWith(SelectionHandle &&rhs)
	{
		m_fieldsSelected.merge(std::move(rhs.m_fieldsSelected));
	}

}
