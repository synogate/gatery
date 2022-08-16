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

#include "../stream/Stream.h"

namespace gtry::scl::bt
{
	struct BehaviorCtrl
	{
		Reverse<Bit> success;
	};

	using BehaviorStream = RvStream<BehaviorCtrl>;

	class Node
	{
	public:
		Node(std::string_view name);
		Node(const Node&) = delete;

		virtual BehaviorStream operator () ();

	protected:
		Area m_area;
		BehaviorStream m_parent;

	private:
		BehaviorStream m_parentIn;
	};

	class Selector : public Node
	{
	public:
		Selector(std::string_view name);

		template<class... TParam>
		Selector(std::string_view name, TParam&&... childs);

		template<std::invocable Func>
		Selector& add(Func&& child) { return add(std::invoke(std::forward<Func>(child))); }

		Selector& add(BehaviorStream& child);
		Selector& add(BehaviorStream&& child) { return add(child); }

	private:
		Bit m_done;
	};

	class Sequence : public Node
	{
	public:
		Sequence(std::string_view name);

		template<class... TParam>
		Sequence(std::string_view name, TParam&&... childs);

		template<std::invocable Func>
		Sequence& add(Func&& child) { return add(std::invoke(std::forward<Func>(child))); }

		Sequence& add(BehaviorStream& child);
		Sequence& add(BehaviorStream&& child) { return add(child); }

	private:
		Bit m_done;
	};

	class Check : public Node
	{
	public:
		Check(std::string_view name = "check");
		Check(const Bit& condition, std::string_view name = "check");

	protected:
		void condition(const Bit& value);
	};

	class Wait : public Node
	{
	public:
		Wait(std::string_view name = "wait");
		Wait(const Bit& condition, std::string_view name = "wait");

	protected:
		void condition(const Bit& value);
	};

	class Do : public Node
	{
	public:
		Do(std::string_view name = "do");
		
		template<std::invocable T>
		Do(T&& handler, std::string_view name = "do");

	protected:
		template<std::invocable T>
		void handler(T&& proc);
	};


}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::bt::BehaviorCtrl, success);

namespace gtry::scl::bt
{
	template<class... TParam>
	Selector::Selector(std::string_view name, TParam&&... childs) :
		Selector(name)
	{
		(add(std::forward<TParam>(childs)), ...);
	}

	template<class... TParam>
	Sequence::Sequence(std::string_view name, TParam&&... childs) :
		Sequence(name)
	{
		(add(std::forward<TParam>(childs)), ...);
	}

	template<std::invocable T>
	Do::Do(T&& proc, std::string_view name) :
		Do(name)
	{
		handler(std::forward<T>(proc));
	}

	template<std::invocable T>
	void Do::handler(T&& proc)
	{
		//TODO: stall scope
		*m_parent->success = dontCare(Bit{});
		IF(valid(m_parent))
			*m_parent->success = std::invoke(std::forward<T>(proc));
	}
}
