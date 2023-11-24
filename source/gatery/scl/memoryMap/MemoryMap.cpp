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
		std::cout << std::string(indent, ' ') << " From: " << desc.offsetInBits << " size " << desc.size << std::endl;
		std::cout << std::string(indent, ' ') << "    Name: " << desc.name << std::endl;
		std::cout << std::string(indent, ' ') << "    Short desc: " << desc.descShort << std::endl;
		std::cout << std::string(indent, ' ') << "    Long desc: " << desc.descLong << std::endl;
		std::cout << std::string(indent, ' ') << "    Children: " << std::endl;
		for (const auto &c : desc.children)
			format(stream, c, indent + 8);
	}

	void MemoryMap::SelectionHandle::joinWith(SelectionHandle &&rhs)
	{
		m_fieldsSelected.merge(std::move(rhs.m_fieldsSelected));
	}

}