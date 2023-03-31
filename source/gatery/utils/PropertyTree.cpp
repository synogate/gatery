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
#include "PropertyTree.h"
#include "Exceptions.h"

namespace gtry::utils
{

#ifdef USE_YAMLCPP

	void YamlPropertyTree::dump(std::ostream& out) const
	{
		out << m_node;
	}

	YamlPropertyTree gtry::utils::YamlPropertyTree::operator[](std::string_view path)
	{
		if (!m_node.IsMap())
			m_node = YAML::Node{ YAML::NodeType::Map };

		YAML::Node child = m_node[std::string(path).c_str()];
		HCL_ASSERT(m_node.IsMap());

		return child;
	}

	void gtry::utils::YamlPropertyTree::push_back(YamlPropertyTree value)
	{
		m_node.push_back(value.m_node);
	}

	bool YamlPropertyTree::empty() const
	{
		if (m_node.IsSequence() && m_node.size() != 0)
			return false;

		if (m_node.IsMap() && m_node.size() != 0)
			return false;

		return true;
	}

	size_t YamlPropertyTree::size() const
	{
		return m_node.size();
	}

	YamlPropertyTree& YamlPropertyTree::operator = (const YamlPropertyTree& val)
	{
		m_node = val.m_node;
		return *this;
	}

	YamlPropertyTree& YamlPropertyTree::operator = (YamlPropertyTree&& val)
	{
		m_node = val.m_node;
		return *this;
	}

	YamlPropertyTree& YamlPropertyTree::operator = (YamlPropertyTree& val)
	{
		m_node = val.m_node;
		return *this;
	}
	
#endif

}
