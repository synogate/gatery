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
	if (highLatencyExternal) {
		forceMemoryResetLogic = true;
		forceNoInitialization = true;
	}

	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = (forceMemoryResetLogic?ClockConfig::ResetType::SYNCHRONOUS:ClockConfig::ResetType::NONE),
			.initializeRegs = false,
			.initializeMemory = !forceNoInitialization,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	Bit increment = pinIn().setName("inc");
	UInt bucketIdx = pinIn(BitWidth::count(numBuckets)).setName("bucketIdx");

	Memory<UInt> histogram(numBuckets, bucketWidth);
	histogram.initZero();
	if (twoCycleLatencyBRam)
		histogram.setType(MemType::MEDIUM, 2);
	if (highLatencyExternal)
		histogram.setType(MemType::EXTERNAL, 10);

	UInt bucketValue = histogram[bucketIdx];
	if (forceNoEnable) {
		bucketValue += ext(mux(increment, std::array<UInt, 2>{"1b0", "1b1"}));
		histogram[bucketIdx] = bucketValue;
	} else
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
		simu(increment) = '0';

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

		co_await AfterClk(clock);
		co_await AfterClk(clock);

		stopTest();
	});

	runTest(Seconds{numBuckets + numBuckets*iterationFactor + numBuckets * (latency+1) + 100,1} / clock.absoluteFrequency());
}

void Test_MemoryCascade::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = (forceMemoryResetLogic?ClockConfig::ResetType::SYNCHRONOUS:ClockConfig::ResetType::NONE),
			.initializeRegs = false,
			.initializeMemory = !forceNoInitialization,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	Bit wrEn = pinIn().setName("wrEn");
	UInt addr = pinIn(BitWidth::count(depth)).setName("addr");
	UInt wrValue = pinIn(elemSize).setName("wrValue");

	Memory<UInt> memory(depth, elemSize);

	IF (wrEn)
		memory[addr] = wrValue;

	UInt readValue = memory[addr];

	size_t latency = memory.readLatencyHint();
	for ([[maybe_unused]] auto i : utils::Range(latency)) {
		readValue = reg(readValue, {.allowRetimingBackward=true});
	}

	pinOut(readValue).setName("readValue");


	addSimulationProcess([&]()->SimProcess {

		std::mt19937 rng;

		std::vector<size_t> addresses;
		addresses.resize(numWrites/4);
		for (auto &v : addresses)
			v = rng() % depth;


		std::vector<size_t> refMemory;
		refMemory.resize(depth, 0);
		std::vector<bool> refMemoryWritten;
		refMemoryWritten.resize(depth, false);
		for ([[maybe_unused]] auto i : gtry::utils::Range(numWrites)) {

			bool doWrite = rng() & 1;
			size_t value = rng() % elemSize.value;
			size_t address = addresses[i % addresses.size()];
			simu(wrEn) = doWrite;
			simu(wrValue) = value;
			simu(addr) = address;
			co_await OnClk(clock);

			if (doWrite) {
				refMemory[address] = value;
				refMemoryWritten[address] = true;
			}
		}
		simu(wrEn) = '0';
		
		for (auto i : addresses) {
			simu(addr) = i;

			co_await WaitStable();

			for ([[maybe_unused]] auto i : gtry::utils::Range(latency))
				co_await AfterClk(clock);

			if (refMemoryWritten[i])
				BOOST_TEST(simu(readValue) == (int) refMemory[i]);

			if (latency == 0)
				co_await AfterClk(clock);
		}

		stopTest();
	});

	runTest(Seconds{numWrites + numWrites/4 * (latency+1) + 100,1} / clock.absoluteFrequency());
}





void Test_SDP_DualClock::execute()
{
	Clock clockA({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = (forceMemoryResetLogic?ClockConfig::ResetType::SYNCHRONOUS:ClockConfig::ResetType::NONE),
			.initializeRegs = false,
			.initializeMemory = !forceNoInitialization,
	});
	HCL_NAMED(clockA);
	Clock clockB({
			.absoluteFrequency = {{260'000'000,1}},
			.memoryResetType = (forceMemoryResetLogic?ClockConfig::ResetType::SYNCHRONOUS:ClockConfig::ResetType::NONE),
			.initializeRegs = false,
			.initializeMemory = !forceNoInitialization,
	});
	HCL_NAMED(clockB);


	ClockScope scp(clockA);

	Bit wrEn = pinIn().setName("wrEn");
	UInt wrAddr = pinIn(BitWidth::count(depth)).setName("rdAddr");
	UInt wrValue = pinIn(elemSize).setName("wrValue");

	Memory<UInt> memory(depth, elemSize);
	memory.noConflicts();

	IF (wrEn)
		memory[wrAddr] = wrValue;


	ClockScope scpB(clockB);

	UInt rdAddr = pinIn(BitWidth::count(depth)).setName("rdAddr");
	UInt readValue = memory[rdAddr];

	size_t latency = memory.readLatencyHint();
	for ([[maybe_unused]] auto i : utils::Range(latency)) {
		readValue = reg(readValue, {.allowRetimingBackward=true});
	}

	pinOut(readValue).setName("readValue");


	addSimulationProcess([&]()->SimProcess {

		std::mt19937 rng;

		std::vector<size_t> refMemory;
		refMemory.resize(depth, 0);
		std::vector<bool> refMemoryWritten;
		refMemoryWritten.resize(depth, false);

		bool keepChecking = true;

		simu(wrEn) = '0';

		co_await OnClk(clockA);
		co_await OnClk(clockB);

		fork([&]()->SimProcess{
			while (keepChecking) {

				size_t address = rng() % depth;
				simu(rdAddr) = address;

				size_t expectedValue = refMemory[address];
				bool expectedDefined = refMemoryWritten[address];
		
				co_await OnClk(clockB);

				fork([&, expectedValue, expectedDefined]()->SimProcess{
					for ([[maybe_unused]] auto i : utils::Range(latency))
						co_await OnClk(clockB);

					if (!expectedDefined)
						BOOST_TEST(!simu(readValue).defined());
					else
						BOOST_TEST(simu(readValue) == expectedValue);
				});
			}
		});


		for ([[maybe_unused]] auto i : gtry::utils::Range(numWrites)) {

			bool doWrite = rng() & 1;
			size_t value = rng() % elemSize.value;
			size_t address = rng() % depth;
			simu(wrEn) = doWrite;
			simu(wrValue) = value;
			simu(wrAddr) = address;
			co_await OnClk(clockA);

			if (doWrite) {
				refMemory[address] = value;
				refMemoryWritten[address] = true;
			}
		}
		simu(wrEn) = '0';
		
		for ([[maybe_unused]] auto i : gtry::utils::Range(100))
			co_await AfterClk(clockA);

		stopTest();
	});

	runTest(Seconds{numWrites + 200,1} / clockA.absoluteFrequency());
}



void Test_ReadEnable::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = ClockConfig::ResetType::NONE,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	Bit wrEn = pinIn().setName("wrEn");
	UInt wrAddr = pinIn(BitWidth::count(numElements)).setName("wrAddr");
	UInt wrData = pinIn(elementWidth).setName("wrData");

	Bit rdEn = pinIn().setName("rdEn");
	UInt rdAddr = pinIn(BitWidth::count(numElements)).setName("rdAddr");
	
	Memory<UInt> memory(numElements, elementWidth);
	if (twoCycleLatencyBRam)
		memory.setType(MemType::MEDIUM, 2);
	if (highLatencyExternal)
		memory.setType(MemType::EXTERNAL, 10);

	UInt rdValue = memory[rdAddr];
	size_t latency = memory.readLatencyHint();
	ENIF(rdEn)
		for ([[maybe_unused]] auto i : utils::Range(latency))
			rdValue = reg(rdValue, {.allowRetimingBackward=true});

	IF (wrEn)
		memory[wrAddr] = wrData;


	pinOut(rdValue).setName("rdValue");


	addSimulationProcess([&]()->SimProcess {
		simu(wrEn) = '0';
	
		simu(rdEn) = '1';
		simu(rdAddr) = 1;

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		simu(rdEn) = '0';

		simu(wrEn) = '1';
		simu(wrAddr) = 1;
		simu(wrData) = 42;

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		
		simu(wrEn) = '0';

		simu(rdEn) = '1';
	
		for ([[maybe_unused]] auto i : utils::Range(latency)) {
			BOOST_TEST(simu(rdValue) != 42);
			co_await AfterClk(clock);
		}

		BOOST_TEST(simu(rdValue) == 42);


		if (latency > 1) {
			simu(rdEn) = '0';
			co_await AfterClk(clock);
			co_await AfterClk(clock);
			co_await AfterClk(clock);
			co_await AfterClk(clock);

			simu(wrEn) = '1';
			simu(wrAddr) = 2;
			simu(wrData) = 10;

			co_await AfterClk(clock);

			simu(wrEn) = '0';
			simu(rdEn) = '1';
			simu(rdAddr) = 2;

			co_await AfterClk(clock);
			
			simu(rdEn) = '0';

			for ([[maybe_unused]] auto i : utils::Range(latency*3)) {
				BOOST_TEST(simu(rdValue) != 10);
				co_await AfterClk(clock);
			}

			simu(rdEn) = '1';

			for ([[maybe_unused]] auto i : utils::Range(latency-1)) {
				BOOST_TEST(simu(rdValue) != 10);
				co_await AfterClk(clock);
			}

			BOOST_TEST(simu(rdValue) == 10);
		}

		co_await AfterClk(clock);
		co_await AfterClk(clock);

		stopTest();
	});

	runTest(Seconds{100, 1} / clock.absoluteFrequency());
}
