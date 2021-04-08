/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "Compound.h"

namespace hcl::core::frontend
{

	void CompoundVisitor::enterPackStruct()
	{
	}

	void CompoundVisitor::enterPackContainer()
	{
	}

	void CompoundVisitor::leavePack()
	{
	}

	void CompoundVisitor::enter(std::string_view name)
	{
	}

	void CompoundVisitor::leave()
	{
	}

	void CompoundVisitor::operator()(const BVec& a, const BVec& b)
	{
	}

	void CompoundVisitor::operator()(BVec& a)
	{
	}

	void CompoundVisitor::operator()(BVec& a, const BVec& b)
	{
	}

	void CompoundVisitor::operator()(const Bit& a, const Bit& b)
	{
	}

	void CompoundVisitor::operator()(Bit& a)
	{
	}

	void CompoundVisitor::operator()(Bit& a, const Bit& b)
	{
	}

	void CompoundNameVisitor::enter(std::string_view name)
	{
		m_names.push_back(name);
		CompoundVisitor::enter(name);
	}

	void CompoundNameVisitor::leave()
	{
		m_names.pop_back();
		CompoundVisitor::leave();
	}

	std::string CompoundNameVisitor::makeName() const
	{
		std::string name;
		for (std::string_view part : m_names)
		{
			if (!name.empty() && !std::isdigit(part.front()))
				name += '_';
			name += part;
		}
		return name;
	}
}
