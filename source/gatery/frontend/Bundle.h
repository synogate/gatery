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
#include "Compound.h"

#include <string_view>
#include <any>
#include <typeinfo>
#include <vector>
#include <functional>

#if 0

namespace gtry
{
	class Bundle
	{
		struct Item
		{
			std::string_view name = "";
			size_t index = 0;
			std::any instance;

			std::function<void(CompoundVisitor& v)> visitCopy;
			std::function<void(CompoundVisitor& v)> visitConst;
			std::function<void(CompoundVisitor& v)> visitMutate;
		};

	public:
		template<typename T> T& get(std::string_view name = "", size_t index = 0);
		template<typename T> const T& get(std::string_view name = "", size_t index = 0) const;
		template<typename T> T& add(T&& item, std::string_view name = "", size_t index = 0);
		template<typename T> bool has(std::string_view name = "", size_t index = 0) const;

	protected:
		Item* find(const std::type_info& type, std::string_view name, size_t index);
		const Item* find(const std::type_info& type, std::string_view name, size_t index) const;

	private:
		std::vector<Item> m_member;
	};
}

#endif