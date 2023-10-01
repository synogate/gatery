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

using namespace boost::unit_test;
using namespace gtry;

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

struct SimpleStruct
{
	gtry::UInt vec = gtry::UInt(3_b);
	gtry::Bit bit;
};

BOOST_HANA_ADAPT_STRUCT(SimpleStruct, vec, bit);

struct RichStruct
{
	SimpleStruct base;
	std::vector<SimpleStruct> list;
	int parameter = 5;
};

BOOST_HANA_ADAPT_STRUCT(RichStruct, base, list, parameter);

BOOST_FIXTURE_TEST_CASE(CompoundName, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	static_assert(Signal<BVec>);
	static_assert(Signal<BVec&>);
	static_assert(Signal<const BVec&>);
	static_assert(Signal<Reverse<BVec>>);
	static_assert(Signal<Reverse<BVec>&>);
	static_assert(Signal<const Reverse<BVec>&>);
	static_assert(Signal<std::vector<BVec>>);
	static_assert(Signal<std::vector<BVec>&>);
	static_assert(Signal<const std::vector<BVec>>);
	static_assert(Signal<const std::vector<BVec>&>);
	static_assert(Signal<RichStruct>);
	static_assert(Signal<RichStruct&>);
	static_assert(Signal<const RichStruct&>);
	static_assert(Signal<std::array<RichStruct, 2>>);
	static_assert(Signal<std::array<RichStruct, 2>&>);
	static_assert(Signal<const std::array<RichStruct, 2>&>);
	static_assert(!CompoundSignal<std::array<gtry::Bit, 7>>);
	static_assert(Signal<const std::array<gtry::Bit, 7>&>);
	static_assert(TupleSignal<const std::array<gtry::Bit, 7>&>);

	Bit bit;
	setName(bit, "bit");
	BOOST_CHECK(bit.getName() == "bit");

	UInt vec{4_b };
	setName(vec, "vec");
	BOOST_CHECK(vec.getName() == "vec");

	std::vector<UInt> vecvec( 3, vec );
	setName(vecvec, "vecvec");
	BOOST_CHECK(vecvec[0].getName() == "vecvec0");
	BOOST_CHECK(vecvec[1].getName() == "vecvec1");
	BOOST_CHECK(vecvec[2].getName() == "vecvec2");

	RichStruct obj;
	obj.list.emplace_back();
	setName(obj, "obj");
	BOOST_CHECK(obj.list[0].vec.getName() == "obj_list0_vec");
}

BOOST_FIXTURE_TEST_CASE(CompoundWidth, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Bit bit;
	BOOST_TEST(width(bit) == 1_b);

	UInt vec{ 4_b };
	BOOST_TEST(width(vec) == 4_b);

	std::vector<UInt> vecvec( 3, vec );
	BOOST_TEST(width(vecvec) == 3 * 4_b);

}

BOOST_FIXTURE_TEST_CASE(CompoundPack, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	{
		Bit bit = '1';
		UInt bitPack = pack(bit);
		sim_assert(bitPack[0] == '1');
	}

	{
		UInt vec = 5;
		UInt vecPack = pack(vec);
		sim_assert(vecPack == 5);
	}

	{
		UInt vec = 5u;
		std::vector<UInt> vecvec( 3, vec );
		UInt vecPack = pack(vecvec);
		sim_assert(vecPack(0, 3_b) == 5u);
		sim_assert(vecPack(3, 3_b) == 5u);
		sim_assert(vecPack(6, 3_b) == 5u);

		UInt vecCat = pack(vecvec);
		sim_assert(vecPack == vecCat);
	}

	eval();
}

BOOST_FIXTURE_TEST_CASE(CompoundUnpack, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	RichStruct in;
	in.base.vec = 5u;
	in.base.bit = '0';
	for (size_t i = 0; i < 7; ++i)
	{
		in.list.emplace_back();
		in.list.back().vec = ConstUInt(i, 3_b);
		in.list.back().bit = i < 4;
	}

	UInt inPacked = pack(in);

	RichStruct out;
	out.list.resize(in.list.size());
	unpack(inPacked, out);

	sim_assert(out.base.vec == 5u) << 'a';
	sim_assert(out.base.bit == '0') << 'b';
	for (size_t i = 0; i < 7; ++i)
	{
		sim_assert(out.list[i].vec == ConstUInt(i, 3_b)) << 'c';
		sim_assert(out.list[i].bit == (i < 4)) << 'd';
	}

	eval();
}

BOOST_FIXTURE_TEST_CASE(ConstructFromSignal, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Bit sbit = '1';
	Bit dbit = constructFrom(sbit);
	sim_assert(sbit == '1');
	sim_assert(dbit == '0');
	dbit = '0';
	
	UInt svec = "0x101A";
	UInt dvec = constructFrom(svec);
	sim_assert(svec == "0x101A");
	sim_assert(dvec == "0x0101");
	dvec = "0x0101";
	
	SimpleStruct sss{
		.vec = "0x1111",
		.bit = '1'
	};
	SimpleStruct dss = constructFrom(sss);
	sim_assert(dss.bit == '0');
	sim_assert(dss.vec == "0x1010");
	dss.bit = '0';
	dss.vec = "0x1010";
   
	std::array<Bit, 1> sa{ '1' };
	std::array<Bit, 1> da = constructFrom(sa);
	sim_assert(da[0] == '0');
	da[0] = '0';

	enum class TestEnum { VAL1, VAL2 };
	Enum<TestEnum> se = TestEnum::VAL1;
	Enum<TestEnum> de = constructFrom(se);
	sim_assert(de == TestEnum::VAL2);
	de = TestEnum::VAL2;

	//design.visualize("test");

	eval();
}

BOOST_FIXTURE_TEST_CASE(ConstructFromCompound, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	std::array<Bit, 4> fixedContainerSrc = { {'1', '0', '1', '1'} };
	std::array<Bit, 4> fixedContainerDst = constructFrom(fixedContainerSrc);
	sim_assert(fixedContainerSrc[0] == '1');

	std::vector<Bit> dynamicContainerSrc = { '1', '0', '1', '1' };
	std::vector<Bit> dynamicContainerDst = constructFrom(dynamicContainerSrc);
	sim_assert(dynamicContainerSrc[0] == '1');

	RichStruct in;
	in.base.vec = 5u;
	in.base.bit = '0';
	in.parameter = 13;
	for (size_t i = 0; i < 7; ++i)
	{
		in.list.emplace_back();
		in.list.back().vec = ConstUInt(i, 3_b);
		in.list.back().bit = i < 4;
	}

	RichStruct out = constructFrom(in);
	BOOST_TEST(in.parameter == out.parameter);

/*
	std::tuple<int, UInt> inTuple{ 42, 13 };
	std::tuple<int, UInt> outTuple = constructFrom(inTuple);

	BOOST_TEST(std::get<0>(outTuple) == std::get<0>(inTuple));
*/

	eval();
}


BOOST_FIXTURE_TEST_CASE(ConstructFromPreservesLoopiness, BoostUnitTestSimulationFixture)
{
	using namespace gtry;


	Bit bit;
	bit = constructFrom(bit);
	sim_assert(bit == '1');
	bit = '1';


	UInt uInt, uIntTemplate;
	uIntTemplate = 32_b;
	uInt = constructFrom(uIntTemplate);
	sim_assert(uInt == 42);
	uInt = 42;


	RichStruct templateStruct;
	templateStruct.base.vec = 5_b;
	templateStruct.base.bit = '0';
	templateStruct.parameter = 12;

	RichStruct strct;
	strct = constructFrom(templateStruct);
	sim_assert(strct.base.vec == 13);
	strct.base.vec = 13;

	eval();
}

BOOST_FIXTURE_TEST_CASE(testFinalCompound, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	RichStruct in;
	in.base.vec = 5u;
	in.base.bit = '0';
	in.parameter = 13;
	for (size_t i = 0; i < 7; ++i)
	{
		in.list.emplace_back();
		in.list.back().vec = 7;
		in.list.back().bit = '1';
	}

	const RichStruct out = final(in);
	sim_assert(out.parameter == 13);

	in.base.vec = 4;
	sim_assert(out.base.vec == 4);
	in.base.bit = '1';
	sim_assert(out.base.bit == '1');

	for (SimpleStruct& it : in.list)
	{
		it.vec = 6;
		it.bit = '0';
	}
	
	for (const SimpleStruct& it : out.list)
	{
		sim_assert(it.vec == 6);
		sim_assert(it.bit == '0');
	}

	eval();
}

BOOST_FIXTURE_TEST_CASE(tapOnCompound, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	{
		RichStruct obj;
		obj.list.emplace_back();
		pinIn(obj, "obj");
		
		tap(obj);
	}
	//design.visualize("tapOnCompound");
	design.postprocess();

	BOOST_TEST(countNodes([](const auto *node) { return dynamic_cast<const hlim::Node_SignalTap*>(node) != nullptr; }) == 4);	
}

struct SubStruct {
	Bit a;
	BVec b;
};

struct MainStruct {
	SubStruct sub1, sub2;
	Bit c;
	BVec d;
};


BOOST_FIXTURE_TEST_CASE(tapOnUnreflectedCompound, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	{
		MainStruct obj;
		obj.sub1.b = 8_b;
		obj.sub2.b = 8_b;
		obj.d = 10_b;
		pinIn(obj, "obj");
		
		tap(obj);
	}
	//design.visualize("tapOnUnreflectedCompound");
	design.postprocess();

	BOOST_TEST(countNodes([](const auto *node) { return dynamic_cast<const hlim::Node_SignalTap*>(node) != nullptr; }) == 6);
}


BOOST_FIXTURE_TEST_CASE(constructFromDownStreamTupleatedMetaVars, gtry::BoostUnitTestSimulationFixture)
{
	struct HelperStruct {
		int i;
		UInt v;
	};

	int j = 5;
	int i = constructFrom(j);
	BOOST_TEST(i == j);


	HelperStruct a;
	a.i = 5;
	auto b = downstream(a);
	auto c = constructFrom(copy(b));
}
