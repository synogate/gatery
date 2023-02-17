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

BOOST_AUTO_TEST_CASE(EnvVarReplacement)
{
	BOOST_TEST(replaceEnvVars("test") == "test");

	BOOST_CHECK_THROW(replaceEnvVars("$(var)"), std::runtime_error);
	BOOST_CHECK_THROW(replaceEnvVars("test$(var)"), std::runtime_error);

#ifdef WIN32
	_putenv("var=str str");
#else
	putenv((char*)"var=str str");
#endif	
	BOOST_TEST(replaceEnvVars("test $(var) tust") == "test str str tust");
}

#ifdef USE_YAMLCPP

BOOST_AUTO_TEST_CASE(ConfigTreePathSearch)
{
	YAML::Node root;

	// multi node
	root["sub1"]["sub2"]["sub3"]["0"] = 5;
	root["sub1"]["sub2/sub3"]["1"] = 6;
	root["sub1/sub2/sub3"]["2"] = 7;
	root["sub1/donotmatch/sub3"]["2"] = 1;
	root["sub1/*/sub3"]["3"] = 8;
	root["sub1/*"]["sub3"]["4"] = "9";

	// overload resolution
	root["sub1"]["sub2"]["sub3"]["overload"] = 1;
	root["sub1"]["sub2/sub3"]["overload"] = 2;

	YamlConfigTree cfg{ root };

	auto node = cfg["sub1/sub2/sub3"];
	BOOST_TEST(node["0"].as(0) == 5);
	BOOST_TEST(node["1"].as(0) == 6);
	BOOST_TEST(node["2"].as(0) == 7);
	BOOST_TEST(node["3"].as(0) == 8);
	BOOST_TEST(node["4"].as<std::string>("0") == "9");
	BOOST_TEST(node["overload"].as(0) == 2);
}

BOOST_AUTO_TEST_CASE(ConfigTreeEnumLoad)
{
	enum class TestEnum
	{
		TE_1,
		TE_2,
		TE_3
	};

	YAML::Node root;
	root["v1"] = "TE_1";
	root["v2"] = "TE_2";
	root["v3"] = "TE_3";
	root["v4"] = "TE_4";
	YamlConfigTree cfg{ root };

	BOOST_TEST((cfg["v1"].as(TestEnum::TE_3) == TestEnum::TE_1));
	BOOST_TEST((cfg["v2"].as(TestEnum::TE_3) == TestEnum::TE_2));
	BOOST_TEST((cfg["v3"].as(TestEnum::TE_1) == TestEnum::TE_3));
	BOOST_CHECK_THROW((cfg["v4"].as(TestEnum::TE_1)), std::runtime_error);
	BOOST_TEST((cfg["v5"].as(TestEnum::TE_1) == TestEnum::TE_1));
}

BOOST_AUTO_TEST_CASE(ConfigTreeLists)
{
	YAML::Node root;
	root.push_back(1);
	root.push_back(2);
	root.push_back(3);

	YamlConfigTree cfg{ root };
	BOOST_TEST(cfg.isSequence());
	BOOST_TEST(cfg.size() == 3);

	BOOST_TEST((cfg[0].as(0) == 1));
	BOOST_TEST((cfg[1].as(0) == 2));
	BOOST_TEST((cfg[2].as(0) == 3));

	size_t check = 1;
	for (const YamlConfigTree& n : cfg)
		BOOST_TEST((n.as(0) == check++));

}

BOOST_AUTO_TEST_CASE(ConfigTreeRecorder)
{
	YAML::Node root;
	root["sub1"]["sub2"]["sub3"]["0"] = 5;
	root["sub1"]["sub2/sub3"]["1"] = 6;
	root["sub1/sub2/sub3"]["2"] = 7;
	root["sub1/donotmatch/sub3"]["2"] = 1;
	root["sub1/*/sub3"]["3"] = 8;
	root["sub1/*"]["sub3"]["4"] = "9";

	YamlConfigTree config{ root };

	YamlPropertyTree recorder;
	recorder[""] = "test";

	config.addRecorder(recorder);

	config["sub1/sub2/sub3"]["0"].as(0);
	config["sub1/sub2/sub3"]["1"].as(0);
	config["sub1/sub2/sub3"]["2"].as(0);
	config["sub1/sub2/sub3"]["3"].as(0);
	config["sub1/sub5/sub3"]["4"].as<std::string>();

	std::ostringstream out;
	recorder.dump(out);
	std::string result = out.str();

	BOOST_TEST(result.find("donotmatch") == std::string::npos);
	BOOST_TEST(result.find("sub2") != std::string::npos);
	BOOST_TEST(result.find("sub5") != std::string::npos);
}

#endif
