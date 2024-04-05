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
#include "BehaviorTree.h"

namespace gtry::scl::bt
{
	Selector::Selector(std::string_view name) :
		Node(name)
	{
		ready(m_parent) = '0';

		HCL_NAMED(m_done);
		m_done = '0';

		m_area.leave();
	}

	Selector& Selector::add(BehaviorStream& child)
	{
		auto scope = m_area.enter();
		valid(child) = '0';

		IF(!m_done)
		{
			child <<= m_parent;

			IF(!ready(child))
				m_done = '1';
			IF(*child->success)
				m_done = '1';
		}
		return *this;
	}

	Sequence::Sequence(std::string_view name) :
		Node(name)
	{
		ready(m_parent) = '0';

		HCL_NAMED(m_done);
		m_done = '0';

		m_area.leave();
	}

	Sequence& Sequence::add(BehaviorStream& child)
	{
		auto scope = m_area.enter();
		valid(child) = '0';

		IF(!m_done)
		{
			child <<= m_parent;

			IF(!ready(child))
				m_done = '1';
			IF(*child->success == '0')
				m_done = '1';
		}
		return *this;
	}

	Node::Node(std::string_view name) :
		m_area(name, true)
	{
		HCL_NAMED(m_parentIn);

		m_parent <<= m_parentIn;
		HCL_NAMED(m_parent);

		// default downstream
		valid(m_parentIn) = '0';

		// default upstream
		ready(m_parent) = '1';
		*m_parent->success = '1';
	}

	BehaviorStream Node::operator() ()
	{
		auto scope = m_area.enter();
		BehaviorStream ret;
		upstream(ret) = upstream(m_parentIn);
		valid(m_parentIn) |= valid(ret);
		HCL_NAMED(ret);
		return ret;
	}

	Check::Check(std::string_view name) :
		Node(name)
	{
		m_area.leave();
	}

	Check::Check(const Bit& value, std::string_view name) :
		Node(name)
	{
		condition(value);
		m_area.leave();
	}

	void Check::condition(const Bit& value)
	{
		*m_parent->success = value;
	}

	Wait::Wait(std::string_view name) :
		Node(name)
	{
		m_area.leave();
	}

	Wait::Wait(const Bit& value, std::string_view name) :
		Node(name)
	{
		condition(value);
		m_area.leave();
	}

	void Wait::condition(const Bit& value)
	{
		ready(m_parent) = value;
	}

	Do::Do(std::string_view name) :
		Node(name)
	{
		m_area.leave();
	}

}

