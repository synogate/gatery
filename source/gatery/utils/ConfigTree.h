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

#include "../frontend/BitWidth.h"

namespace gtry::utils
{
	std::optional<std::string_view> globbingMatchPath(std::string_view pattern, std::string_view str);
	std::string replaceEnvVars(const std::string& src);

	class DummyConfigTree
	{
	public:

		DummyConfigTree operator[](std::string_view path) const { return {}; }

		bool isDefined() const { return false; }
		bool isNull() const { return false; }
		bool isScalar() const { return false; }
		bool isSequence() const { return false; }
		bool isMap() const { return false; }

		explicit operator bool() const { return isDefined(); }

		//	struct iterator {
		//		YAML::Node::iterator it;
		//
		//		void operator++() { ++it; }
		//		YamlConfigTree operator*() { return YamlConfigTree(*it); }
		//		bool operator==(const iterator &rhs) const { return it == rhs.it; }
		//		bool operator!=(const iterator &rhs) const { return it == rhs.it; }
		//	};
		//
		//	inline auto begin() { return iterator{m_node.begin()}; }
		//	inline auto end() { return iterator{m_node.end()}; }

		template<typename T> T as(const T& def) const { return def; }

		void loadFromFile(const std::filesystem::path& filename) {}
	};
}

#ifdef USE_YAMLCPP

namespace gtry::utils 
{

	class YamlConfigTree 
	{
	public:
		YamlConfigTree() = default;
		YamlConfigTree(YAML::Node node) { m_nodes.push_back(node); }

		YamlConfigTree operator[](std::string_view path);

		bool isDefined() const;
		bool isNull() const;
		bool isScalar() const;
		bool isSequence() const;
		bool isMap() const;

		explicit operator bool() const { return isDefined(); }

	//	struct iterator {
	//		YAML::Node::iterator it;
	//
	//		void operator++() { ++it; }
	//		YamlConfigTree operator*() { return YamlConfigTree(*it); }
	//		bool operator==(const iterator &rhs) const { return it == rhs.it; }
	//		bool operator!=(const iterator &rhs) const { return it == rhs.it; }
	//	};
	//
	//	inline auto begin() { return iterator{m_node.begin()}; }
	//	inline auto end() { return iterator{m_node.end()}; }

		template<typename T> T as(const T& def) const;

		//int get(const std::string &name, int def) { return get(name, def, IntTranslator()); }
		//size_t get(const std::string &name, size_t def) { return get(name, def, SizeTTranslator()); }
		//bool get(const std::string &name, bool def) { return get(name, def, BoolTranslator()); }
		//BitWidth get(const std::string &name, BitWidth def) { return get(name, def, BitWidthTranslator()); }
		//std::string get(const std::string &name, const std::string &def) { return get(name, def, StringTranslator()); }
		//std::string get(const std::string &name, const char *def) { return get(name, def, StringTranslator()); }

		//template<typename T>
		//typename T::Type get(const std::string &name, const typename T::Type &def, T translator) 
		//{
		//	for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it)
		//	{
		//		YAML::Node node = (*it)[name];
		//		if(node)
		//			return translator(node.as<typename T::YamlType>());
		//	}
		//	return def;
		//}


		void loadFromFile(const std::filesystem::path &filename);

	protected:
		std::vector<YAML::Node> m_nodes;
	};

	template<typename T>
	inline T YamlConfigTree::as(const T& def) const
	{
		if (m_nodes.size() != 1)
			return def;

		return m_nodes.front().as<T>();
	}

	template<>
	inline std::string YamlConfigTree::as(const std::string& def) const
	{
		std::string ret;
		if (m_nodes.size() != 1)
			ret = def;
		else
			ret = m_nodes.front().as<std::string>();

		return replaceEnvVars(ret);
	}

	using ConfigTree = YamlConfigTree;
}

namespace YAML
{
	template<>
	struct convert<gtry::BitWidth>
	{
		static bool decode(const Node& node, gtry::BitWidth& out)
		{
			out = gtry::BitWidth{ node.as<uint64_t>() };
			return true;
		}
	};

	template<typename T>
	struct convert
	{
		static auto decode(const Node& node, T& out) -> std::enable_if_t<std::is_enum_v<T>, bool>
		{
			const std::string value = node.as<std::string>();
			const std::optional<T> eval = magic_enum::enum_cast<T>(value);
			if (eval)
			{
				out = *eval;
				return true;
			}
			else
			{
				std::ostringstream err;
				err << "unknown value '" << value << "' for enum " << magic_enum::enum_type_name<T>()
					<< ". Valid values are:";
				for (auto&& name : magic_enum::enum_names<T>())
					err << ' ' << name;
				err << '\n';

				throw std::runtime_error{ err.str() };
			}
			return false;
		}
	};
}

#else

namespace gtry::utils
{
	using ConfigTree = DummyConfigTree;
}

#endif // USE_YAMLCPP
