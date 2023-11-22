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
#include "Bundle.h"


#if 0


namespace gtry
{
	Bundle::Item* gtry::Bundle::find(const std::type_info& type, std::string_view name, size_t index)
	{
		for (Item& it : m_member)
		{
			if (it.index == index && it.name == name && it.instance.type() == type)
				return &it;
		}

		return nullptr;
	}

	const Bundle::Item* gtry::Bundle::find(const std::type_info& type, std::string_view name, size_t index) const
	{
		for (const Item& it : m_member)
		{
			if (it.index == index && it.name == name && it.instance.type() == type)
				return &it;
		}

		return nullptr;
	}

}

#endif