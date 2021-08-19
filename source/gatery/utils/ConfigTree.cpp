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


	YamlConfigTree YamlConfigTree::operator[](std::string_view path)
	{
		PathMatcher	m;
		for(YAML::Node& n : m_nodes)
			m(n, path);

		YamlConfigTree ret;
		ret.m_nodes = m.matches;
		return ret;
	}

	void YamlConfigTree::loadFromFile(const std::filesystem::path &filename)
	{
		m_nodes.push_back(YAML::LoadFile(filename.string()));
	}

	void YamlConfigTree::saveToFile(const std::filesystem::path &filename)
	{
	}
}
