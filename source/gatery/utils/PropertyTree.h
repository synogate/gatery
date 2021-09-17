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

namespace gtry::utils
{
	class YamlPropertyTree
	{
	public:
		YamlPropertyTree() = default;
		YamlPropertyTree(YamlPropertyTree&&) = default;
		YamlPropertyTree(const YamlPropertyTree&) = default;

		void dump(std::ostream&) const;

		YamlPropertyTree operator[](std::string_view path);

		template<typename T>
		YamlPropertyTree& operator = (T&&);
		YamlPropertyTree& operator = (const YamlPropertyTree& val);
		YamlPropertyTree& operator = (YamlPropertyTree&& val);
		YamlPropertyTree& operator = (YamlPropertyTree& val);

		auto begin() const { return m_node.begin(); }
		auto end() const { return m_node.end(); }

		void push_back(YamlPropertyTree value);
		bool empty() const;
		size_t size() const;
	protected:
		YamlPropertyTree(YAML::Node node) : m_node(std::move(node)) {}
		YAML::Node m_node{ YAML::NodeType::Map };
	};

	template<typename T>
	inline YamlPropertyTree& YamlPropertyTree::operator=(T&& val)
	{
		m_node = val;
		return *this;
	}

	inline std::ostream& operator << (std::ostream& s, const YamlPropertyTree& tree)
	{
		tree.dump(s);
		return s;
	}

	using PropertyTree = YamlPropertyTree;
}
