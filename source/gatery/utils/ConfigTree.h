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
#include "PropertyTree.h"

#include <optional>
#include <string>
#include <string_view>
#include <filesystem>

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

		using map_iterator = const DummyConfigTree*;
		map_iterator mapBegin() const { return nullptr; }
		map_iterator mapEnd() const { return nullptr; }

		using iterator = const DummyConfigTree*;
		iterator begin() const { return nullptr; }
		iterator end() const { return nullptr; }
		size_t size() const { return 0; }
		DummyConfigTree operator[](size_t index) const { return {}; }

		template<typename T> T as(const T& def) const { return def; }
		template<typename T> T as() const { throw std::runtime_error{ "get unknown config tree" }; }

		void loadFromFile(const std::filesystem::path& filename) {}

		void addRecorder(PropertyTree recorder) { }
	};
}

#ifdef USE_YAMLCPP

namespace gtry::utils 
{

	class YamlConfigTree 
	{
	public:
		struct iterator {
			const YamlConfigTree& src;
			YAML::Node::const_iterator it;

			void operator++() { ++it; }
			YamlConfigTree operator*() { return YamlConfigTree(*it, src.m_recorder); }
			bool operator==(const iterator& rhs) const { return it == rhs.it; }
			bool operator!=(const iterator& rhs) const { return it != rhs.it; }
		};

		struct map_iterator
		{
			const YamlConfigTree& src;
			YAML::Node::const_iterator it;

			void operator++() { ++it; }
			YamlConfigTree operator*() { return YamlConfigTree(it->second, src.m_recorder); }
			bool operator==(const map_iterator& rhs) const { return it == rhs.it; }
			bool operator!=(const map_iterator& rhs) const { return it != rhs.it; }

			std::string key() const { return it->first.as<std::string>(); }
		};

	public:
		YamlConfigTree() = default;
		YamlConfigTree(YAML::Node node, const std::vector<PropertyTree>& recorder = {});

		void addRecorder(PropertyTree recorder);

		explicit operator bool() const { return isDefined(); }
		bool isDefined() const;
		bool isNull() const;
		bool isScalar() const;
		bool isSequence() const;
		bool isMap() const;

		map_iterator mapBegin() const;
		map_iterator mapEnd() const;

		iterator begin() const;
		iterator end() const;
		size_t size() const;
		YamlConfigTree operator[](size_t index) const;

		YamlConfigTree operator[](std::string_view path) const;
		template<typename T> T as(const T& def) const;
		template<typename T> T as() const;

		void loadFromFile(const std::filesystem::path &filename);

	protected:
		std::vector<YAML::Node> m_nodes;
		mutable std::vector<PropertyTree> m_recorder;
	};

	template<typename T>
	inline T YamlConfigTree::as(const T& def) const
	{
		if (m_nodes.size() != 1)
		{
			for (auto& rec : m_recorder)
				rec = def;
			return def;
		}

		T ret;
		try {
			ret = m_nodes.front().as<T>();
		} catch (...) {
			auto str = m_nodes.front().as<std::string>();
			if (str.empty() || str[0] != '$')
				throw;
			str = replaceEnvVars(str);
			if constexpr (std::is_same_v<T, bool>) {
				if (str == "false" || str == "No")
					ret = false;
				else if (str == "true" || str == "Yes")
					ret = true;
				else 
					throw;
			} else
				if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>)
					ret = boost::lexical_cast<T>(str);
				else throw;
		}
		for (auto& rec : m_recorder)
			rec = ret;
		return std::move(ret);
	}

	template<typename T>
	inline T YamlConfigTree::as() const
	{
		if (m_nodes.size() != 1)
			throw std::runtime_error{ "non optional config value not found" };

		T ret = m_nodes.front().as<T>();
		for (auto& rec : m_recorder)
			rec = ret;
		return std::move(ret);
	}

	template<>
	inline std::string YamlConfigTree::as(const std::string& def) const
	{
		std::string ret;
		if (m_nodes.size() != 1)
			ret = def;
		else
			ret = m_nodes.front().as<std::string>();

		ret = replaceEnvVars(ret);
		for (auto& rec : m_recorder)
			rec = ret;
		return ret;
	}

	template<>
	inline std::string YamlConfigTree::as() const
	{
		if (m_nodes.size() != 1)
			throw std::runtime_error{ "non optional config value not found" };

		std::string ret = m_nodes.front().as<std::string>();
		ret = replaceEnvVars(ret);
		for (auto& rec : m_recorder)
			rec = ret;
		return ret;
	}

	using ConfigTree = YamlConfigTree;
}

namespace YAML
{
	template<>
	struct convert<gtry::BitWidth>
	{
		static Node encode(gtry::BitWidth rhs)
		{
			return Node{ rhs.bits() };
		}

		static bool decode(const Node& node, gtry::BitWidth& out)
		{
			out = gtry::BitWidth{ node.as<uint64_t>() };
			return true;
		}
	};

	template<typename T>
	struct convert
	{
		static auto encode(T value) -> std::enable_if_t<std::is_enum_v<T>, Node>
		{
			return Node{ std::string{ magic_enum::enum_name(value) } };
		}


		static auto decode(const Node& node, T& out) -> std::enable_if_t<std::is_enum_v<T>, bool>
		{
			const std::string value = node.as<std::string>();
			const std::optional<T> eval = magic_enum::enum_cast<T>(value,
				[](char a, char b) { return std::tolower(a) == std::tolower(b); });

			if (eval)
			{
				out = *eval;
				return true;
			}
			else
			{
				std::ostringstream err;
				err << "unknown value '" << value << "' for enum " << magic_enum::enum_type_name<T>()
					<< ". Valid values are ";

				auto names = magic_enum::enum_names<T>();
				for (size_t i = 0; i < names.size(); ++i)
				{
					if (i == names.size() - 1 && names.size() > 1)
						err << " or ";
					else if (i != 0)
						err << ", ";
					err << names[i];
				}
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
