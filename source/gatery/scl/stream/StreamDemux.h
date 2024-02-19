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

#include "Stream.h"

namespace gtry::scl
{

	template<StreamSignal T>
	class StreamDemux
	{
	public:
		StreamDemux(T&& in) : m_in(move(in)) { init(); }
		StreamDemux(T&& in, const UInt& selector) : m_selector(selector), m_in(move(in)) { init(); HCL_NAMED(m_selector); }

		StreamDemux& selector(const UInt& selector) { m_selector = selector; auto ent = m_area.enter(); HCL_NAMED(m_selector); return *this; }

		T out(size_t index)
		{
			auto ent = m_area.enter();

			T out = constructFrom(m_in);
			downstream(out) = downstream(m_in);
			valid(out) = '0';

			IF(index == zext(m_selector))
			{
				valid(out) = valid(m_in);
				upstream(m_in) = upstream(out);
			}

			setName(out, "out_" + std::to_string(index));
			return out;
		}

		Vector<T> out()
		{
			auto ent = m_area.enter();
			
			Vector<T> out;
			out.reserve(m_selector.width().count());
			for (size_t i = 0; i < m_selector.width().count(); ++i)
				out.emplace_back(this->out(i));

			return out;
		}

	protected:
		void init()
		{
			ready(m_in) = '1'; // default is to consume everything pointing at unconnected outputs
			HCL_NAMED(m_in);

			m_area.leave();
		}

		Area m_area = { "scl_StreamDemux", true };
		UInt m_selector;
		T m_in;
	};
}
