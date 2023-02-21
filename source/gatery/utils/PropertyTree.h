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


#if __has_include(<yaml-cpp/yaml.h>)
# include <yaml-cpp/yaml.h>
# include <external/magic_enum.hpp>
# define USE_YAMLCPP
#else
# pragma message ("yaml-cpp not found. compiling without config file support")
#endif

#include <string_view>
#include <ostream>

namespace gtry::utils
{
	class DummyPropertyTree
	{
	public:
		DummyPropertyTree operator[](std::string_view path) const { return {}; }

		void dump(std::ostream&) const { }

		DummyPropertyTree() = default;
		DummyPropertyTree(DummyPropertyTree&&) = default;
		DummyPropertyTree(const DummyPropertyTree&) = default;

		DummyPropertyTree operator[](std::string_view path) { return {}; }

		template<typename T>
		DummyPropertyTree& operator = (T&&) { return *this; }
		DummyPropertyTree& operator = (const DummyPropertyTree& val) { return *this; }
		DummyPropertyTree& operator = (DummyPropertyTree&& val) { return *this; }
		DummyPropertyTree& operator = (DummyPropertyTree& val) { return *this; }

		using iterator = const DummyPropertyTree*;
		iterator begin() const { return nullptr; }
		iterator end() const { return nullptr; }

		void push_back(DummyPropertyTree value) { }
		bool empty() const { return true; }
		size_t size() const { return 0; }

		template<typename T> T as(const T& def) const { return def; }
		template<typename T> T as() const { throw std::runtime_error{ "get unknown prop tree" }; }
	};
}


namespace gtry::utils
{

#ifdef USE_YAMLCPP

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

		template<typename T> T as(const T& def) const;
		template<typename T> T as() const;
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

	template<typename T>
	inline T YamlPropertyTree::as(const T& def) const
	{
		return m_node.as<T>(def);
	}

	template<typename T>
	inline T YamlPropertyTree::as() const
	{
		return m_node.as<T>();
	}

	template<>
	inline std::string YamlPropertyTree::as(const std::string& def) const
	{
		std::string ret;
		if (m_node.IsNull())
			ret = def;
		else
			ret = m_node.as<std::string>();

		//ret = replaceEnvVars(ret);
		return ret;
	}

	template<>
	inline std::string YamlPropertyTree::as() const
	{
		std::string ret = m_node.as<std::string>();
		//ret = replaceEnvVars(ret);
		return ret;
	}


	using PropertyTree = YamlPropertyTree;
#else
	using PropertyTree = DummyPropertyTree;
#endif
}
