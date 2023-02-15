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

#include "ConfigTree.h"

#include <boost/spirit/home/x3.hpp>
#include <regex>
#include <fstream>


namespace gtry::utils 
{
	std::optional<std::string_view> globbingMatchPath(std::string_view pattern, std::string_view str)
	{
		size_t patternPos = 0;
		while (patternPos < pattern.size() && patternPos < str.size() && str[patternPos] == pattern[patternPos])
			patternPos++;

		if (patternPos == pattern.size())
			return str.substr(0, patternPos);

		if (pattern[patternPos] != '*')
			return std::nullopt;

		std::optional<std::string_view> best_match;
		for (size_t strPos = patternPos; strPos <= str.size(); ++strPos)
		{
			std::string_view sub_str = str.substr(strPos);
			auto match = globbingMatchPath(pattern.substr(patternPos + 1), sub_str);
			if (match)
				best_match = str.substr(0, strPos + match->size());

			if (sub_str.starts_with('/'))
				break;
		}
		return best_match;
	}

	std::string replaceEnvVars(const std::string& src)
	{
		using namespace boost::spirit::x3;

		std::string ret;
		ret.reserve(src.size());

		auto append_var = [&](auto& ctx) {
			const char* var_name = _attr(ctx).c_str();
			const char* var = getenv(var_name);
			if (!var)
				throw std::runtime_error(std::string("environment variable '") + var_name + "' not found.");
			_attr(ctx) = var;
		};

		auto parser = *((lit('$') >> '(' >> (*(char_ - ')'))[append_var] >> ')') | char_);
		bool valid = parse(src.cbegin(), src.cend(), parser, ret);
		HCL_ASSERT(valid);
		return ret;
	}

#ifdef USE_YAMLCPP

	struct PathMatcher
	{
		void operator () (YAML::Node node, std::string_view path)
		{
			if (!node.IsMap())
				return;

			for (auto it = node.begin(); it != node.end(); ++it)
			{
				if (!it->second.IsMap())
					continue;

				const std::string key = it->first.as<std::string>();
				auto match = globbingMatchPath(key, path);
				if (match && match->size() == path.size())
				{
					matches.push_back(it->second);
				}
				else if (match && path[match->size()] == '/')
				{
					(*this)(it->second, path.substr(match->size() + 1));
				}
			}
		}

		std::vector<YAML::Node> matches;
	};

	YamlConfigTree YamlConfigTree::operator[](std::string_view path) const
	{
		YamlConfigTree ret;
		for (auto& recorder : m_recorder)
			ret.addRecorder(recorder[path]);

		for(auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it)
		{
			YAML::Node element = (*it)[std::string{ path }];
			if (element && !element.IsMap())
			{
				ret.m_nodes.push_back(element);
				break;
			}
		}

		if (ret.m_nodes.empty())
		{
			PathMatcher	m;
			for (const YAML::Node& n : m_nodes)
				m(n, path);

			ret.m_nodes = std::move(m.matches);
		}
		return ret;
	}

	YamlConfigTree::YamlConfigTree(YAML::Node node, const std::vector<PropertyTree>& recorder) :
		m_nodes{node},
		m_recorder(recorder)
	{
	}

	void YamlConfigTree::addRecorder(PropertyTree recorder)
	{
		m_recorder.push_back(std::move(recorder));
	}

	bool YamlConfigTree::isDefined() const
	{
		return !m_nodes.empty() && m_nodes.front().IsDefined();
	}

	bool YamlConfigTree::isNull() const
	{
		return m_nodes.size() == 1 && m_nodes.front().IsNull();
	}

	bool YamlConfigTree::isScalar() const
	{
		return m_nodes.size() == 1 && m_nodes.front().IsScalar();
	}

	bool YamlConfigTree::isSequence() const
	{
		return m_nodes.size() == 1 && m_nodes.front().IsSequence();
	}

	bool YamlConfigTree::isMap() const
	{
		return !m_nodes.empty() && m_nodes.front().IsMap();
	}

	YamlConfigTree::map_iterator YamlConfigTree::mapBegin() const
	{
		if (isMap())
		{
			HCL_ASSERT_HINT(m_nodes.size() == 1, "no impl");
			return map_iterator{ *this, m_nodes.front().begin() };
		}
		return map_iterator{*this};
	}

	YamlConfigTree::map_iterator YamlConfigTree::mapEnd() const
	{
		if (isMap())
		{
			HCL_ASSERT_HINT(m_nodes.size() == 1, "no impl");
			return map_iterator{ *this, m_nodes.front().end() };
		}
		return map_iterator{ *this };
	}

	YamlConfigTree::iterator YamlConfigTree::begin() const
	{
		if (isSequence())
			return iterator{ *this, m_nodes.front().begin() };
		return iterator{ *this };
	}

	YamlConfigTree::iterator YamlConfigTree::end() const
	{
		if(isSequence())
			return iterator{ *this, m_nodes.front().end() };
		return iterator{ *this };
	}

	size_t YamlConfigTree::size() const
	{
		if (isSequence())
			return m_nodes.front().size();
		return 0;
	}

	YamlConfigTree YamlConfigTree::operator[](size_t index) const
	{
		if (isSequence())
			return YamlConfigTree{ m_nodes.front()[index], m_recorder };
		return YamlConfigTree();
	}

	void YamlConfigTree::loadFromFile(const std::filesystem::path &filename)
	{
		m_nodes.push_back(YAML::LoadFile(filename.string()));

		if (m_nodes.size() > 1 && !m_nodes.back().IsMap())
			throw std::runtime_error(filename.string() + " is not a yaml map");
	}

#endif // USE_YAMLCPP
}
