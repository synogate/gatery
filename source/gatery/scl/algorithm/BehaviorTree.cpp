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
#include "gatery/pch.h"
#include "BehaviorTree.h"

namespace gtry::scl::bt
{
	Selector& Selector::add(BehaviorStream& child)
	{
	

		Selector a{};

		a.add(a);
		a.add(Selector{});

		Selector b{
			a,
			Selector{},
			a
		};

		return *this;
	}

	CheckCondition::CheckCondition(const Bit& condition, std::string name)
	{
		auto scope = Area{ name }.enter();
		m_condition = condition;
		setName(m_condition, "condition");
	}

	BehaviorStream CheckCondition::operator() () const
	{
		BehaviorStream out{ {
			.success = m_condition,
			.stick = '0'
		} };

		ready(out) = '1';
		return out;
	}
}

