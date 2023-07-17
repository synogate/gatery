/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

using namespace boost::unit_test;

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;



BOOST_FIXTURE_TEST_CASE(PartitionTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	YAML::Node tree;
	{
		auto inst = tree["instance"];
		inst["myArea*"]["partition"] = true;
	}
	
	gtry::utils::ConfigTree configTree(tree);
	{
		Area myArea("notPartition", true);
		gtry::hlim::NodeGroup::configTree(configTree["instance"]);


		{
			Area myArea("myArea", true);

			//BOOST_TEST(GroupScope::get()->isPartition());
			BOOST_TEST(myArea.isPartition());

			{
				Area myArea("myInnerArea", true);
				BOOST_TEST(myArea.getNodeGroup()->getParent()->getName() == "myArea");
				BOOST_TEST(myArea.getNodeGroup()->getParent()->isPartition());
				BOOST_TEST(myArea.getNodeGroup()->getName() == "myArea_myInnerArea");
			}

		}
	}
}

