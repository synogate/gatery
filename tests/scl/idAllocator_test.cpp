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

#include <gatery/scl/idAllocator.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

using namespace boost::unit_test;
using namespace gtry;

struct idAllocatorTestFixture : protected BoostUnitTestSimulationFixture {

	Clock m_clk = Clock({ .absoluteFrequency = 100'000'000 });

	BitWidth m_txIdW = 4_b;

	scl::VStream<UInt> inFreeIDStream = { m_txIdW };
	scl::RvStream<UInt> outIDStream = { m_txIdW };

	void prepareTest() {
		ClockScope clkscp(m_clk);

		outIDStream = scl::idAllocator(inFreeIDStream);

		pinIn(inFreeIDStream, "in_packet");
		pinOut(outIDStream, "out");

		addSimulationProcess([&, this]()->SimProcess {
			simu(valid(inFreeIDStream)) = '0';
			co_return;
		});
	}
	SimProcess freeTxId(size_t txid) { return sendPacket(inFreeIDStream, scl::strm::SimPacket(txid, m_txIdW), m_clk); }
};

BOOST_FIXTURE_TEST_CASE(idalloc_simpletest, idAllocatorTestFixture) {
	prepareTest();

	addSimulationProcess([&, this]()->SimProcess {
		size_t numIDs = 3;

		fork(scl::strm::readyDriver(outIDStream, m_clk));

		for (size_t i = 0; i < numIDs; i++)
		{
			scl::strm::SimPacket receivedPacket = co_await scl::strm::receivePacket(outIDStream, m_clk);
			auto value = receivedPacket.asUint64(m_txIdW);
			BOOST_TEST(value == i);
		}

		stopTest();
	});
	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(idalloc_get_all_IDs, idAllocatorTestFixture) {
	prepareTest();

	addSimulationProcess([&, this]()->SimProcess {
		fork(scl::strm::readyDriver(outIDStream, m_clk));

		for (size_t i = 0; i < m_txIdW.count(); i++)
		{
			scl::strm::SimPacket receivedPacket = co_await scl::strm::receivePacket(outIDStream, m_clk);
			auto value = receivedPacket.asUint64(m_txIdW);
			BOOST_TEST(value == i);
		}

		co_await OnClk(m_clk);
		BOOST_TEST(simu(valid(outIDStream)) == '0');

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(idalloc_get_all_free_all_get_all, idAllocatorTestFixture) {
	prepareTest();

	addSimulationProcess([&, this]()->SimProcess {
		simu(ready(outIDStream)) = '1';

		for (size_t i = 0; i < m_txIdW.count(); i++)
		{
			scl::strm::SimPacket receivedPacket = co_await scl::strm::receivePacket(outIDStream, m_clk);
			auto value = receivedPacket.asUint64(m_txIdW);
			BOOST_TEST(value == i);
		}

		co_await OnClk(m_clk);

		for (size_t i = 0; i < 10; i++)
		{
			BOOST_TEST(simu(valid(outIDStream)) == '0');

			simu(ready(outIDStream)) = '0';
			for (int i = (int)m_txIdW.count() - 1; i >= 0; i--)
			{
				co_await scl::strm::sendPacket(inFreeIDStream, scl::strm::SimPacket(i, m_txIdW), m_clk);
			}

			simu(ready(outIDStream)) = '1';
			for (int i = (int)m_txIdW.count() - 1; i >= 0; i--)
			{
				scl::strm::SimPacket receivedPacket = co_await scl::strm::receivePacket(outIDStream, m_clk);
				auto value = receivedPacket.asUint64(m_txIdW);
				BOOST_TEST(value == i);
			}
			co_await OnClk(m_clk);
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(idalloc_fuzzing, idAllocatorTestFixture) {
	prepareTest();

	addSimulationProcess([&, this]()->SimProcess {
		co_await scl::strm::readyDriverRNG(outIDStream, m_clk, 50);
	});

	addSimulationProcess([&, this]()->SimProcess {
		std::vector<size_t> idInFlight;
		std::mt19937 rng{ 5434 };
		
		fork([&, this]()->SimProcess {
			while (true){
				size_t freeProbability = 50;
				while (idInFlight.empty() or (rng() % 100 > freeProbability)) {
					co_await OnClk(m_clk);
				}
				size_t idx = rng() % idInFlight.size();
				co_await freeTxId(idInFlight[idx]);
				idInFlight[idx] = idInFlight.back();
				idInFlight.pop_back();
			}
		});

		for (size_t i = 0; i < 512; i++){
			co_await performTransferWait(outIDStream, m_clk);
					
			for (auto id : idInFlight){
				BOOST_TEST(simu(*outIDStream) != id);
			}
			idInFlight.push_back(simu(*outIDStream));
		}

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(idAllocator_test, ClockedTest)
{
	size_t numIds = 7;

	auto in = std::make_shared<scl::VStream<UInt>>(4_b);
	auto out = std::make_shared<scl::RvStream<UInt>>(scl::idAllocator(*in, numIds));

	pinIn(*in, "in");
	pinOut(*out, "out");

	addSimulationProcess([=, this]()->SimProcess {
		simu(valid(*in)) = '0';
		simu(ready(*out)) = '0';
		co_await OnClk(clock());

		// lets allocate numIds ids
		simu(ready(*out)) = '1';
		co_await OnClk(clock());

		for (size_t i = 0; i < numIds; ++i)
		{
			BOOST_TEST(simu(valid(*out)) == '1');
			BOOST_TEST(simu(**out) == i);
			co_await OnClk(clock());
		}
		BOOST_TEST(simu(valid(*out)) == '0');
		simu(ready(*out)) = '0';

		// free ids in reverse order 
		simu(valid(*in)) = '1';
		for (size_t i = 0; i < numIds; ++i)
		{
			simu(**in) = numIds - 1 - i;
			co_await OnClk(clock());
		}
		simu(valid(*in)) = '0';

		// allocate and check again
		simu(ready(*out)) = '1';
		co_await OnClk(clock());

		for (size_t i = 0; i < numIds; ++i)
		{
			BOOST_TEST(simu(valid(*out)) == '1');
			BOOST_TEST(simu(**out) == numIds - 1 - i);
			co_await OnClk(clock());
		}
		BOOST_TEST(simu(valid(*out)) == '0');
		simu(ready(*out)) = '0';

		co_await OnClk(clock());
		stopTest();
	});
}

BOOST_FIXTURE_TEST_CASE(id_allocator_in_order, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	BitWidth idW = 3_b;
	Bit freeId = pinIn().setName("freeId");
	auto out = scl::idAllocatorInOrder(freeId, idW.count());
	pinOut(out, "out"); 

	size_t numIDs = 0;

	addSimulationProcess([&, this]()->SimProcess {
		fork(scl::strm::readyDriverRNG(out, clk, 80));

		for(size_t i = 0; i < 100; ++i)
		{
			co_await performTransferWait(out, clk);
			BOOST_TEST(simu(*out) == i % idW.count());
			numIDs++;
		}
		stopTest();
	});


	addSimulationProcess([&, this]()->SimProcess {
		std::mt19937 rng{ 5434 };
		while (true)
		{
			simu(freeId) = '0';
			while(numIDs == 0 || rng() % 2 == 0)
				co_await OnClk(clk);
		
			simu(freeId) = '1';
			numIDs--;
			co_await OnClk(clk);
		}
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
} 
