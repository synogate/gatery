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

#include "MappingTests_Memory.h"

#include <boost/test/unit_test.hpp>

using namespace gtry;

void Test_Histogram::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	Bit increment = pinIn().setName("inc");
	UInt bucketIdx = pinIn(BitWidth::count(numBuckets)).setName("bucketIdx");

	Memory<UInt> histogram(numBuckets, bucketWidth);
	histogram.initZero();

	UInt bucketValue = histogram[bucketIdx];
	IF (increment) {
		bucketValue += 1;
		histogram[bucketIdx] = bucketValue;
	}

	size_t latency = histogram.readLatencyHint();
	for ([[maybe_unused]] auto i : utils::Range(latency)) {
		bucketValue = reg(bucketValue, {.allowRetimingBackward=true});
	}

	pinOut(bucketValue).setName("bucketValue");


	addSimulationProcess([&]()->SimProcess {
		std::vector<size_t> histCopy;
		histCopy.resize(numBuckets, 0);
		std::mt19937 rng;
		for ([[maybe_unused]] auto i : gtry::utils::Range(numBuckets*iterationFactor)) {

			bool doInc = rng() & 1;
			size_t incAddr = rng() % numBuckets;
			simu(increment) = doInc;
			simu(bucketIdx) = incAddr;
			co_await OnClk(clock);

			if (doInc)
				histCopy[incAddr]++;
		}
		simu(increment) = '0';
		
		for (auto i : gtry::utils::Range(numBuckets)) {
			simu(bucketIdx) = i;

			co_await WaitStable();

			for ([[maybe_unused]] auto i : gtry::utils::Range(latency))
				co_await AfterClk(clock);

			BOOST_TEST(simu(bucketValue) == (int) histCopy[i]);

			if (latency == 0)
				co_await AfterClk(clock);
		}

		stopTest();
	});

	runTest(Seconds{numBuckets + numBuckets*iterationFactor + numBuckets * (latency+1) + 100,1} / clock.absoluteFrequency());
}