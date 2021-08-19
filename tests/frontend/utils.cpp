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
#include "frontend/pch.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/utils/ConfigTree.h>

using namespace boost::unit_test;
using namespace gtry::utils;

BOOST_AUTO_TEST_CASE(GlobbingMatchPath)
{
	{
		auto m1 = globbingMatchPath("full_match", "full_match");
		BOOST_TEST((m1 && *m1 == "full_match"));
	}
	{
		auto m1 = globbingMatchPath("full", "full_match");
		BOOST_TEST((m1 && *m1 == "full"));
	}
	{
		auto m1 = globbingMatchPath("not", "full_match");
		BOOST_TEST((!m1));
	}
	{
		auto m1 = globbingMatchPath("a/b", "a/bc");
		BOOST_TEST((m1 && *m1 == "a/b"));
	}
	{
		auto m1 = globbingMatchPath("*", "a/bc");
		BOOST_TEST((m1 && *m1 == "a"));
	}
	{
		auto m1 = globbingMatchPath("a/*", "a/bc");
		BOOST_TEST((m1 && *m1 == "a/bc"));
	}
	{
		auto m1 = globbingMatchPath("a/*", "a/bc/c");
		BOOST_TEST((m1 && *m1 == "a/bc"));
	}
	{
		auto m1 = globbingMatchPath("a/b*", "a/bc/c");
		BOOST_TEST((m1 && *m1 == "a/bc"));
	}
	{
		auto m1 = globbingMatchPath("a/*c", "a/bc");
		BOOST_TEST((m1 && *m1 == "a/bc"));
	}
	{
		auto m1 = globbingMatchPath("a/b*", "a/bc");
		BOOST_TEST((m1 && *m1 == "a/bc"));
	}
	{
		auto m1 = globbingMatchPath("a/b*c", "a/bc");
		BOOST_TEST((m1 && *m1 == "a/bc"));
	}
	{
		auto m1 = globbingMatchPath("a/b*/*", "a/b/c");
		BOOST_TEST((m1 && *m1 == "a/b/c"));
	}
}


BOOST_AUTO_TEST_CASE(ConfigTreePathSearch)
{
	YAML::Node root;

	// multi node
	root["sub1"]["sub2"]["sub3"]["0"] = 5;
	root["sub1"]["sub2/sub3"]["1"] = 6;
	root["sub1/sub2/sub3"]["2"] = 7;
	root["sub1/donotmatch/sub3"]["2"] = 1;
	root["sub1/*/sub3"]["3"] = 8;
	root["sub1/*"]["sub3"]["4"] = 9;

	// overload resolution
	root["sub1"]["sub2"]["sub3"]["overload"] = 1;
	root["sub1"]["sub2/sub3"]["overload"] = 2;

	YamlConfigTree cfg{ root };

	auto node = cfg["sub1/sub2/sub3"];
	BOOST_TEST(node.get("0", 0) == 5);
	BOOST_TEST(node.get("1", 0) == 6);
	BOOST_TEST(node.get("2", 0) == 7);
	BOOST_TEST(node.get("3", 0) == 8);
	BOOST_TEST(node.get("4", 0) == 9);
	BOOST_TEST(node.get("overload", 0) == 2);
}
