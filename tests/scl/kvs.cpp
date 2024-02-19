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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

extern "C"
{
#include <gatery/scl/kvs/TinyCuckooDriver.h>
}

using namespace boost::unit_test;
using namespace gtry;


BOOST_DATA_TEST_CASE_F(gtry::BoostUnitTestSimulationFixture, TinyCookuTableLookup, data::xrange(2, 4), numTables)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clockScope(clock);

	const BitWidth keySize{ size_t(numTables * 4) };
	const BitWidth tableIdxWidth{ gtry::utils::Log2C(size_t(numTables)) };

	InputPins lookupKey = pinIn(keySize).setName("key");
	InputPin update = pinIn().setName("update");
	InputPins updateTableIdx = pinIn(tableIdxWidth).setName("updateTableIdx");
	InputPins updateItemIdx = pinIn(4_b).setName("updateItemIdx");
	InputPin updateItemValid = pinIn().setName("updateItemValid");
	InputPins updateItemKey = pinIn(keySize).setName("updateItemKey");
	InputPins updateItemValue = pinIn(8_b).setName("updateItemValue");
	
	scl::TinyCuckooIn params = {
		.key = lookupKey,
		.hash = lookupKey,
		.userData = 0,
		.update = {
			.valid = update,
			.tableIdx = updateTableIdx,
			.itemIdx = updateItemIdx,
			.item = {
				.valid = updateItemValid,
				.key = updateItemKey,
				.value = updateItemValue
			}
		},
		.numTables = size_t(numTables),
	};
	HCL_NAMED(params);
	scl::TinyCuckooOut result = scl::tinyCuckoo(params);

	OutputPin outFound = pinOut(result.found).setName("found");
	OutputPins outValue = pinOut(result.value).setName("value");

	const size_t invalid = std::numeric_limits<size_t>::max();
	std::vector<std::vector<std::pair<size_t, size_t>>> state{
		params.numTables,
		std::vector<std::pair<size_t, size_t>>{
			1ull << params.tableWidth().value,
			std::make_pair(invalid, invalid)
		}
	};

	addSimulationProcess([&]()->SimProcess {
		std::mt19937 rng{ 1337 };
		simu(update) = '0';

		while(true)
		{
			size_t value = rng() & 0xFF;
			size_t key = gtry::utils::bitfieldExtract(value * 23, 0, keySize.value);

			if (rng() % 3 == 0)
			{
				const size_t tableIdx = key % params.numTables;
				const size_t itemIdx = (key >> (tableIdx * 4)) & 0xF;

				simu(update) = '1';
				simu(updateItemKey) = key;
				simu(updateItemValue) = value;
				simu(updateTableIdx) = tableIdx;
				simu(updateItemIdx) = itemIdx;

				if (rng() % 5 == 0)
				{
					simu(updateItemValid) = '0';
					state[tableIdx][itemIdx] = std::make_pair(invalid, invalid);
				}
				else
				{
					simu(updateItemValid) = '1';
					state[tableIdx][itemIdx] = std::make_pair(key, value);
				}
			}

			co_await AfterClk(clock);
			simu(update) = '0';
		}
	});

	addSimulationProcess([&]()->SimProcess {
		std::mt19937 rng{ 1338 };
		while (true)
		{
			simu(lookupKey) = gtry::utils::bitfieldExtract(rng(), 0, keySize.value);
			co_await AfterClk(clock);
		}
	});

	addSimulationProcess([&]()->SimProcess {

		std::deque<size_t> lookupQueue;

		while (true)
		{
			co_await OnClk(clock);
			if (lookupQueue.size() == params.latency)
			{
				if (simu(outFound))
					BOOST_TEST(lookupQueue.back() == simu(outValue));
				else
					BOOST_TEST(lookupQueue.back() == invalid);
				lookupQueue.pop_back();
			}

			const size_t key = simu(lookupKey);
			size_t tableKey = key;
			size_t value = invalid;
			for (const auto& t : state)
			{
				const auto& item = t[tableKey & 0xF];
				tableKey >>= 4;
				if (item.first == key)
					value = item.second;
			}
			lookupQueue.push_front(value);
		}
	});


	design.postprocess();
	//design.visualize("TinyCookuTableLookup");
	//sim::VCDSink vcd(design.getCircuit(), getSimulator(), "TinyCookuTableLookup.vcd");
	//vcd.addAllNamedSignals();

	runTicks(clock.getClk(), 4096);
}


BOOST_DATA_TEST_CASE_F(gtry::BoostUnitTestSimulationFixture, TinyCuckooTableLookup, data::xrange(3, 4), numTables)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clockScope(clock);

	const BitWidth keySize{ size_t(numTables * 10) };
	InputPins lookupKey = pinIn(keySize).setName("key");

	scl::TinyCuckoo<UInt, UInt> tc{ size_t(numTables * 1024), keySize, 4_b, size_t(numTables) };
	BOOST_TEST(keySize.value == tc.hashWidth().value);

	auto cuckooPoo = tc(lookupKey, lookupKey);
	cuckooPoo = reg(cuckooPoo);
	pinOut(cuckooPoo.found).setName("out_found");
	pinOut(cuckooPoo.value).setName("out_value");

	scl::AvalonNetworkSection net;
	tc.addCpuInterface(net);
	net.assignPins();
	

	//design.visualize("TinyCookuTableLookup_before");
	//design.postprocess();
	//design.visualize("TinyCookuTableLookup");
	//sim::VCDSink vcd(design.getCircuit(), getSimulator(), "TinyCookuTableLookup.vcd");
	//vcd.addAllNamedSignals();

	//gtry::vhdl::VHDLExport vhdl("vhdl/");
	//vhdl(design.getCircuit());

	runTicks(clock.getClk(), 4096);
}

BOOST_DATA_TEST_CASE_F(gtry::BoostUnitTestSimulationFixture, TinyCuckooTableLookupDemuxed, data::xrange(3, 4), numTables)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clockScope(clock);

	const BitWidth keySize{ size_t(numTables * 10) };
	InputPins lookupKey = pinIn(keySize).setName("key");

	scl::TinyCuckoo<UInt, UInt> tc{ size_t(numTables * 1024), keySize, 4_b, size_t(numTables) };
	BOOST_TEST(keySize.value == tc.hashWidth().value);

	auto cuckooPoo = tc(lookupKey, lookupKey);
	cuckooPoo = reg(cuckooPoo, {.allowRetimingBackward=true});
	pinOut(cuckooPoo.found).setName("out_found");
	pinOut(cuckooPoo.value).setName("out_value");

	scl::AvalonNetworkSection net;
	tc.addCpuInterface(net);
	scl::AvalonMM ctrl = net.demux();
	net.clear();

	ctrl.pinIn("ctrl");


	//design.visualize("TinyCuckooTableLookupDemuxed_before");
	design.postprocess();
	//design.visualize("TinyCuckooTableLookupDemuxed");
	//sim::VCDSink vcd(design.getCircuit(), getSimulator(), "TinyCuckooTableLookupDemuxed.vcd");
	//vcd.addAllNamedSignals();

	//gtry::vhdl::VHDLExport vhdl("vhdl/");
	//vhdl(design.getCircuit());

	runTicks(clock.getClk(), 4096);
}

BOOST_AUTO_TEST_CASE(TinyCuckooDriverBaseTest)
{
	TinyCuckooContext* ctx = tiny_cuckoo_init(32 * 1024, 4, 32, 32, 
		driver_alloc, driver_free);
	BOOST_TEST(ctx);

	MmTestCtx mmCtx;
	tiny_cuckoo_set_mm(ctx, MmTestWrite, &mmCtx);
	tiny_cuckoo_set_hash(ctx, driver_basic_hash, NULL);

	uint32_t testKey = 128;
	uint32_t testVal = 1337;
	BOOST_TEST(!tiny_cuckoo_lookup(ctx, &testKey));

	BOOST_TEST(tiny_cuckoo_update(ctx, &testKey, &testVal));
	BOOST_TEST(mmCtx.mem.size() == 4);
	BOOST_TEST(mmCtx.mem[0] == 128);
	BOOST_TEST(mmCtx.mem[1] == 1);
	BOOST_TEST(mmCtx.mem[2] == testKey);
	BOOST_TEST(mmCtx.mem[3] == testVal);

	uint32_t* lookupVal = tiny_cuckoo_lookup(ctx, &testKey);
	BOOST_TEST(lookupVal);
	BOOST_TEST(*lookupVal == testVal);

	testVal = ~testVal;
	BOOST_TEST(tiny_cuckoo_update(ctx, &testKey, &testVal));

	lookupVal = tiny_cuckoo_lookup(ctx, &testKey);
	BOOST_TEST(lookupVal);
	BOOST_TEST(*lookupVal == testVal);

	BOOST_TEST(tiny_cuckoo_remove(ctx, &testKey));
	BOOST_TEST(!tiny_cuckoo_remove(ctx, &testKey));
	BOOST_TEST(!tiny_cuckoo_lookup(ctx, &testKey));

	tiny_cuckoo_destroy(ctx);
}

BOOST_DATA_TEST_CASE(TinyCuckooDriverFuzzTest, data::xrange(0, 3), tableShift)
{
	const uint32_t numTables = 2 << tableShift;
	TinyCuckooContext* ctx = tiny_cuckoo_init(64 * 1024, numTables, 32, 32, 
		driver_alloc, driver_free);
	BOOST_TEST(ctx);

	MmTestCtx mmCtx;
	tiny_cuckoo_set_mm(ctx, MmTestWrite, &mmCtx);
	tiny_cuckoo_set_hash(ctx, driver_basic_hash, NULL);

	std::map<uint32_t, uint32_t> ref;
	const auto seed = std::random_device{}();
	std::mt19937 rng{ seed };

	for (size_t i = 0;; ++i)
	{
		uint32_t key = rng() & 0xFFFFF;
		uint32_t val = rng();

		uint32_t* uut = tiny_cuckoo_lookup(ctx, &key);
		auto it_ref = ref.find(key);
		if (it_ref == ref.end())
		{
			BOOST_TEST(uut == nullptr, "seed: " << seed);
			int ret = tiny_cuckoo_remove(ctx, &key);
			BOOST_TEST(ret == 0);
		}
		else
		{
			BOOST_TEST(uut, "seed: " << seed);
			BOOST_TEST(*uut = it_ref->second, "seed: " << seed);

			if (i % 3 == 0)
			{
				int ret = tiny_cuckoo_remove(ctx, &key);
				BOOST_TEST(ret != 0);
				ref.erase(key);
			}
		}

		if (!tiny_cuckoo_update(ctx, &key, &val))
			break;
		ref[key] = val;
	}

	size_t capacityExpectation = ctx->capacity / (numTables == 2 ? 5 : 2);
	BOOST_TEST(ref.size() > capacityExpectation,
		"reached only " << ref.size() / double(ctx->capacity) << " of capacity using seed: " << seed << ", tables: " << numTables);

	for (std::pair<uint32_t, uint32_t> kvp : ref)
	{
		const uint32_t* it = tiny_cuckoo_lookup(ctx, &kvp.first);
		BOOST_TEST(it);
		BOOST_TEST(*it == kvp.second);
	}

	const uint32_t* item_end = ctx->items + ctx->capacity * ctx->itemWords;
	for (uint32_t* item = ctx->items; item != item_end; item += ctx->itemWords)
	{
		if (item[0])
		{
			auto it_ref = ref.find(item[1]);
			bool is_end = it_ref == ref.end();
			BOOST_TEST(!is_end);
			if (!is_end)
			{
				BOOST_TEST(it_ref->second == item[2]);
				ref.erase(it_ref);
			}
		}
	}
	BOOST_TEST(ref.empty());
	BOOST_TEST(mmCtx.mem.size() == 4);

	tiny_cuckoo_destroy(ctx);
}

