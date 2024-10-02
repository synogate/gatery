/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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
#include "AddressSpaceDescription.h"
#include <gatery/scl/driver/MemoryMapEntry.h>


namespace gtry
{
	void connectAddrDesc(scl::AddressSpaceDescriptionHandle &lhs, scl::AddressSpaceDescriptionHandle &rhs)
	{
		if (rhs == nullptr)
			rhs = std::make_shared<scl::AddressSpaceDescription>();

		if (lhs == nullptr)
			lhs = rhs;
		else 
			lhs->children = {{ 0, rhs }};
	}
}

namespace gtry::scl
{

	bool AddressSpaceDescription::isForwardingElement() const
	{
		return 
			size == 0_b &&
			name.empty() &&
			descShort.empty() &&
			descLong.empty() &&
			children.size() == 1 &&
			children.front().offsetInBits == 0;
	}

	AddressSpaceDescription *AddressSpaceDescription::getNonForwardingElement()
	{
		if (isForwardingElement()) {
			HCL_ASSERT(children.front().desc.get() != this);
			return children.front().desc->getNonForwardingElement();
		} else
			return this;
	}

	const AddressSpaceDescription *AddressSpaceDescription::getNonForwardingElement() const
	{
		if (isForwardingElement())
			return children.front().desc->getNonForwardingElement();
		else
			return this;
	}

	/*
    std::size_t hash_value(AddressSpaceDescription const& d)
    {
		std::size_t hash = 31337;
		boost::hash_combine(hash, d.size.bits());
		//boost::hash_combine(hash, d.flags);
		boost::hash_combine(hash, d.name);
		boost::hash_combine(hash, d.descShort);
		boost::hash_combine(hash, d.descLong);
		for (const auto &c : d.children) { // this is probably a bad idea
			boost::hash_combine(hash, c.offsetInBits);
			boost::hash_combine(hash, c.desc);
		}
        return hash;
    }
	*/

	void format(std::ostream &stream, const AddressSpaceDescription &desc, size_t indent, std::uint64_t offset)
	{
		if (desc.isForwardingElement())
			format(stream, *desc.children.front().desc, indent, offset);
		else {
			std::cout << std::string(indent, ' ') << " From: " << (offset / 8) << " (byte) size " << desc.size << std::endl;
			std::cout << std::string(indent, ' ') << "    Name: " << desc.name << std::endl;
			std::cout << std::string(indent, ' ') << "    Short desc: " << desc.descShort << std::endl;
			std::cout << std::string(indent, ' ') << "    Long desc: " << desc.descLong << std::endl;
			std::cout << std::string(indent, ' ') << "    Children: " << std::endl;
			for (const auto &c : desc.children)
				format(stream, *c.desc, indent + 8, offset + c.offsetInBits);
		}
	}

	std::tuple<FlatAddressSpaceDescription, AddressSpaceDescriptionHandle> exportAddressSpaceDescription(AddressSpaceDescriptionHandle desc)
	{
		std::vector<driver::MemoryMapEntry> result;


		/*
			The MemoryMapEntry::childrenStart member points to the index of the first child, which are then stored sequentially in the array.
			To achieve this ordering, we explore the graph breath first. When inserting an entry, we keep don't yet know the childrenStart index,
			so we keep an array mapping from the first child back to the parent to set its childrenStart index when creating that first child.

			The map maps from the descriptor o the child to the parent's index in the result array.
		*/
		std::map<const AddressSpaceDescription*, size_t> childrenStartBackpointers;

		std::list<std::pair<const AddressSpaceDescription*, std::uint64_t>> queue;
		queue.push_back({ desc->getNonForwardingElement(), 0 });
		while (!queue.empty()) {
			auto [d, offset] = queue.front();
			queue.pop_front();

			auto it = childrenStartBackpointers.find(d);
			if (it != childrenStartBackpointers.end())
				result[it->second].childrenStart = (std::uint32_t) result.size();

			if (!d->children.empty())
				childrenStartBackpointers[d->children.front().desc->getNonForwardingElement()] = result.size();

			driver::MemoryMapEntry entry = {
				.addr = offset,
				.width = d->size.value,
				.flags = 0,
				.name = d->name.c_str(),
				.shortDesc = d->descShort.c_str(),
				.longDesc = d->descLong.c_str(),
				.childrenStart = 0,
				.childrenCount = (std::uint32_t) d->children.size(),
			};

			result.push_back(entry);

			for (const auto &c : d->children)
				queue.push_back({ c.desc->getNonForwardingElement(), offset + c.offsetInBits });
		}

		return { std::move(result), std::move(desc) };
	}

	void format(std::ostream &stream, std::string_view name, std::span<const driver::MemoryMapEntry> memoryMap)
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

	void writeGTKWaveFilterFile(std::ostream &stream, std::span<const driver::MemoryMapEntry> memoryMap)
	{
		std::function<void(const std::string &prefix, const driver::MemoryMapEntry &)> reccurse;

		reccurse = [&](const std::string &prefix, const driver::MemoryMapEntry &e) {
			std::string fullName;
			if (prefix.empty())
				fullName = e.name;
			else
				fullName = prefix+'_'+e.name;

			if (e.childrenCount == 0 && e.width > 0) {
				stream << std::hex << e.addr/8 << " " << fullName << std::endl;
			}
			for (size_t i = 0; i < e.childrenCount; i++)
				reccurse(fullName, memoryMap[e.childrenStart + i]);
		};

		if (!memoryMap.empty())
			reccurse("", memoryMap[0]);
	}
}
