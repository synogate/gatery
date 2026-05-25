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

#include <gatery/hlim/postprocessing/MemoryStorage.h>

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




void Test_CrossPortOrdering::execute()
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

	UInt rdAddr = pinIn(BitWidth::count(numElements)).setName("rdAddr");
	
	Memory<UInt> memory(numElements, elementWidth);
	if (declareNoConflict)
		memory.noConflicts();
	memory.setType(memType, latency);

	if (!readFirst)
		IF (wrEn)
			memory[wrAddr] = wrData;

	UInt rdValue = memory[rdAddr];
	for ([[maybe_unused]] auto i : utils::Range(latency))
		rdValue = reg(rdValue, {.allowRetimingBackward=true});

	if (readFirst)
		IF (wrEn)
			memory[wrAddr] = wrData;


	pinOut(rdValue).setName("rdValue");


	addSimulationProcess([&,this]()->SimProcess {

		std::mt19937 rng(1337);
		std::uniform_int_distribution<size_t> addrDist(0, numElements-1);
		std::uniform_int_distribution<size_t> writeDataDist(0, elementWidth.last());
		std::bernoulli_distribution doWriteDist(0.5f);

		hlim::MemoryStorageDense refMemory(numElements * elementWidth.bits());

		struct Read {
			size_t readAddr;
			sim::DefaultBitVectorState expected;
		};
		std::queue<Read> readQueue;
		for ([[maybe_unused]] auto i : utils::Range(latency))
			readQueue.push({});

		simu(wrEn) = '0';
		for ([[maybe_unused]] auto iter : utils::Range(1000)) {
			co_await AfterClk(clock);

			size_t writeAddr = addrDist(rng);
			size_t writeData = writeDataDist(rng);
			bool doWrite = doWriteDist(rng);

			size_t readAddr = addrDist(rng);


			simu(wrEn) = doWrite;
			simu(wrAddr) = writeAddr;
			simu(wrData) = writeData;
			simu(rdAddr) = readAddr;

			co_await WaitStable();

			auto performWrite = [&]{
				if (doWrite) {
					refMemory.write(
						writeAddr * elementWidth.bits(), 
						sim::parseBitVector(writeData, elementWidth.bits()),
						false, {});
				}
			};
			
			if (!readFirst)
				performWrite();

			if (declareNoConflict && readAddr == writeAddr)
				readQueue.push({});
			else
				readQueue.push(Read{ .readAddr = readAddr, .expected = refMemory.read(readAddr * elementWidth.bits(), elementWidth.bits())});

			auto read = std::move(readQueue.front());
			readQueue.pop();

			if (read.expected.size() != 0) {
				BOOST_TEST_MESSAGE("In cycle " << iter);
				BOOST_TEST_MESSAGE("Read from " << read.readAddr << " expected " << read.expected);
				if (readFirst)
					BOOST_TEST_MESSAGE("Writing to " << writeAddr << " with data " << writeData << " after");
				else
					BOOST_TEST_MESSAGE("Wrote to " << writeAddr << " with data " << writeData << " before");
				BOOST_TEST(simu(rdValue) == read.expected);
			}

			if (readFirst)
				performWrite();
		}
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		stopTest();
	});

	runTest(Seconds{2000, 1} / clock.absoluteFrequency());
}


void Test_MaskedWrite::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = ClockConfig::ResetType::NONE,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	size_t maskWidth = elementWidth.bits() / maskGranularity;
	HCL_DESIGNCHECK(elementWidth.bits() % maskGranularity == 0);

	Bit wrEn = pinIn().setName("wrEn");
	UInt wrAddr = pinIn(BitWidth::count(numElements)).setName("wrAddr");
	UInt wrData = pinIn(elementWidth).setName("wrData");
	UInt wrMask = pinIn(BitWidth(maskWidth)).setName("wrMask");

	UInt rdAddr = pinIn(BitWidth::count(numElements)).setName("rdAddr");

	Memory<UInt> memory(numElements, elementWidth);
	memory.setType(memType, latency);
	if (declareNoConflict)
		memory.noConflicts();

	UInt rdValue;

	if (readFirst)
		rdValue = memory[rdAddr];

	IF (wrEn)
		memory[wrAddr] = std::make_pair(wrData, wrMask);

	if (!readFirst)
		rdValue = memory[rdAddr];

	for ([[maybe_unused]] auto i : utils::Range(latency))
		rdValue = reg(rdValue, {.allowRetimingBackward=true});

	pinOut(rdValue).setName("rdValue");

	addSimulationProcess([&,this]()->SimProcess {

		std::mt19937 rng(42);
		std::uniform_int_distribution<size_t> addrDist(0, numElements-1);
		std::uniform_int_distribution<size_t> dataDist(0, elementWidth.last());
		std::uniform_int_distribution<size_t> maskDist(0, (1ull << maskWidth) - 1);
		std::bernoulli_distribution doWriteDist(0.5f);

		hlim::MemoryStorageDense refMemory(numElements * elementWidth.bits());

		struct Read {
			size_t readAddr;
			sim::DefaultBitVectorState expected;
		};
		std::queue<Read> readQueue;
		for ([[maybe_unused]] auto i : utils::Range(latency))
			readQueue.push({});

		simu(wrEn) = '0';
		for ([[maybe_unused]] auto iter : utils::Range(1000)) {
			co_await AfterClk(clock);

			size_t writeAddr = addrDist(rng);
			size_t writeData = dataDist(rng);
			size_t writeMask = maskDist(rng);
			bool doWrite = doWriteDist(rng);

			size_t readAddr;
			do {
				readAddr = addrDist(rng);
			} while (declareNoConflict && readAddr == writeAddr);

			simu(wrEn) = doWrite;
			simu(wrAddr) = writeAddr;
			simu(wrData) = writeData;
			simu(wrMask) = writeMask;
			simu(rdAddr) = readAddr;

			co_await WaitStable();

			if (readFirst)
				readQueue.push(Read{ .readAddr = readAddr, .expected = refMemory.read(readAddr * elementWidth.bits(), elementWidth.bits())});

			if (doWrite) {
				// Build a bit-level mask from the word enable mask
				sim::DefaultBitVectorState bitMask;
				bitMask.resize(elementWidth.bits());
				for (size_t i = 0; i < maskWidth; i++)
					for (size_t b = 0; b < maskGranularity; b++) {
						bitMask.set(sim::DefaultConfig::DEFINED, i * maskGranularity + b, true);
						bitMask.set(sim::DefaultConfig::VALUE, i * maskGranularity + b, (writeMask >> i) & 1);
					}
				refMemory.write(
					writeAddr * elementWidth.bits(),
					sim::parseBitVector(writeData, elementWidth.bits()),
					false, bitMask);
			}

			if (!readFirst)
				readQueue.push(Read{ .readAddr = readAddr, .expected = refMemory.read(readAddr * elementWidth.bits(), elementWidth.bits())});

			auto read = std::move(readQueue.front());
			readQueue.pop();

			if (read.expected.size() != 0) {
				BOOST_TEST_MESSAGE("In cycle " << iter << " at time " << toDouble(getCurrentSimulationTime()) << 's');
				BOOST_TEST_MESSAGE("Read from " << read.readAddr << " expected " << read.expected);
				BOOST_TEST(simu(rdValue) == read.expected);
			}
		}
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		stopTest();
	});

	runTest(Seconds{2000, 1} / clock.absoluteFrequency());
}


void Test_TrueDualPort::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = ClockConfig::ResetType::NONE,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	Bit wrEn1 = pinIn().setName("wrEn1");
	UInt addr1 = pinIn(BitWidth::count(numElements)).setName("addr1");
	UInt wrData1 = pinIn(elementWidth).setName("wrData1");

	Bit wrEn2 = pinIn().setName("wrEn2");
	UInt addr2 = pinIn(BitWidth::count(numElements)).setName("addr2");
	UInt wrData2 = pinIn(elementWidth).setName("wrData2");

	Memory<UInt> memory(numElements, elementWidth);
	if (declareNoConflict)
		memory.noConflicts();
	memory.setType(memType, latency);

	UInt rdValue1 = ConstUInt(elementWidth);
	IF (wrEn1)
		memory[addr1] = wrData1;
	ELSE
		rdValue1 = memory[addr1];

	/*
		todo: also check:

		rdValue1 = memory[addr1];
		IF (wrEn1)
			rdValue1 = ConstUInt(elementWidth);
	*/
/*
	ASSUME(addr1 != addr2);
	GUARANTEE(addr1 != addr2);
*/
	UInt rdValue2 = ConstUInt(elementWidth);
	IF (wrEn2)
		memory[addr2] = wrData2;
	ELSE
		rdValue2 = memory[addr2];

	/*
		Check that this also works with:
		IF (wrEn2)
			memory[addr2(0, 2_b)] = wrData2;
		ELSE
			rdValue2 = memory[addr2(0, 2_b)];

		
		Log: Write TDP decisions
	*/


	for ([[maybe_unused]] auto i : utils::Range(latency)) {
		rdValue1 = reg(rdValue1, {.allowRetimingBackward=true});
		rdValue2 = reg(rdValue2, {.allowRetimingBackward=true});
	}

	pinOut(rdValue1).setName("rdValue1");
	pinOut(rdValue2).setName("rdValue2");

	addSimulationProcess([&,this]()->SimProcess {

		std::mt19937 rng(4711);
		std::uniform_int_distribution<size_t> addrDist(0, numElements-1);
		std::uniform_int_distribution<size_t> dataDist(0, elementWidth.last());
		std::bernoulli_distribution doWriteDist(0.5f);

		hlim::MemoryStorageDense refMemory(numElements * elementWidth.bits());

		struct Read {
			bool active = false;
			size_t readAddr = 0;
			sim::DefaultBitVectorState expected;
		};
		std::queue<Read> readQueue1;
		std::queue<Read> readQueue2;
		for ([[maybe_unused]] auto i : utils::Range(latency)) {
			readQueue1.push({});
			readQueue2.push({});
		}

		simu(wrEn1) = '0';
		simu(wrEn2) = '0';
		for ([[maybe_unused]] auto iter : utils::Range(1000)) {
			co_await AfterClk(clock);

			bool doWrite1 = doWriteDist(rng);
			bool doWrite2 = doWriteDist(rng);
			size_t addrA = addrDist(rng);
			size_t addrB;
			// Avoid conflicting cross-port accesses to the same address
			// (would yield undefined behavior under noConflicts and is implementation-defined otherwise).
			do {
				addrB = addrDist(rng);
			} while (addrA == addrB && (doWrite1 || doWrite2));
			size_t writeData1 = dataDist(rng);
			size_t writeData2 = dataDist(rng);

			simu(wrEn1) = doWrite1;
			simu(addr1) = addrA;
			simu(wrData1) = writeData1;

			simu(wrEn2) = doWrite2;
			simu(addr2) = addrB;
			simu(wrData2) = writeData2;

			co_await WaitStable();

			auto doWrite = [&](size_t a, size_t d) {
				refMemory.write(
					a * elementWidth.bits(),
					sim::parseBitVector(d, elementWidth.bits()),
					false, {});
			};

			if (doWrite1) doWrite(addrA, writeData1);
			if (doWrite2) doWrite(addrB, writeData2);

			Read r1{};
			if (!doWrite1) {
				r1.active = true;
				r1.readAddr = addrA;
				r1.expected = refMemory.read(addrA * elementWidth.bits(), elementWidth.bits());
			}
			Read r2{};
			if (!doWrite2) {
				r2.active = true;
				r2.readAddr = addrB;
				r2.expected = refMemory.read(addrB * elementWidth.bits(), elementWidth.bits());
			}
			readQueue1.push(r1);
			readQueue2.push(r2);

			auto checkOne = [&](std::queue<Read> &q, UInt &readSig, const char *portName) {
				auto read = std::move(q.front());
				q.pop();
				if (read.active) {
					BOOST_TEST_MESSAGE("In cycle " << iter << " at time " << toDouble(getCurrentSimulationTime()) << "s on " << portName);
					BOOST_TEST_MESSAGE("Read from " << read.readAddr << " expected " << read.expected);
					BOOST_TEST(simu(readSig) == read.expected);
				}
			};
			checkOne(readQueue1, rdValue1, "port1");
			checkOne(readQueue2, rdValue2, "port2");
		}
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		stopTest();
	});

	runTest(Seconds{2000, 1} / clock.absoluteFrequency());
}


void Test_TrueDualPortMaskedWrite::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = ClockConfig::ResetType::NONE,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	size_t maskWidth = elementWidth.bits() / maskGranularity;
	HCL_DESIGNCHECK(elementWidth.bits() % maskGranularity == 0);

	Bit wrEn1 = pinIn().setName("wrEn1");
	UInt addr1 = pinIn(BitWidth::count(numElements)).setName("addr1");
	UInt wrData1 = pinIn(elementWidth).setName("wrData1");
	UInt wrMask1 = pinIn(BitWidth(maskWidth)).setName("wrMask1");

	Bit wrEn2 = pinIn().setName("wrEn2");
	UInt addr2 = pinIn(BitWidth::count(numElements)).setName("addr2");
	UInt wrData2 = pinIn(elementWidth).setName("wrData2");
	UInt wrMask2 = pinIn(BitWidth(maskWidth)).setName("wrMask2");

	Memory<UInt> memory(numElements, elementWidth);
	if (declareNoConflict)
		memory.noConflicts();
	memory.setType(memType, latency);

	UInt rdValue1 = ConstUInt(elementWidth);
	IF (wrEn1)
		memory[addr1] = std::make_pair(wrData1, wrMask1);
	ELSE
		rdValue1 = memory[addr1];

	UInt rdValue2 = ConstUInt(elementWidth);
	IF (wrEn2)
		memory[addr2] = std::make_pair(wrData2, wrMask2);
	ELSE
		rdValue2 = memory[addr2];

	for ([[maybe_unused]] auto i : utils::Range(latency)) {
		rdValue1 = reg(rdValue1, {.allowRetimingBackward=true});
		rdValue2 = reg(rdValue2, {.allowRetimingBackward=true});
	}

	pinOut(rdValue1).setName("rdValue1");
	pinOut(rdValue2).setName("rdValue2");

	addSimulationProcess([&,this]()->SimProcess {

		std::mt19937 rng(8128);
		std::uniform_int_distribution<size_t> addrDist(0, numElements-1);
		std::uniform_int_distribution<size_t> dataDist(0, elementWidth.last());
		std::uniform_int_distribution<size_t> maskDist(0, (1ull << maskWidth) - 1);
		std::bernoulli_distribution doWriteDist(0.5f);

		hlim::MemoryStorageDense refMemory(numElements * elementWidth.bits());

		struct Read {
			bool active = false;
			size_t readAddr = 0;
			sim::DefaultBitVectorState expected;
		};
		std::queue<Read> readQueue1;
		std::queue<Read> readQueue2;
		for ([[maybe_unused]] auto i : utils::Range(latency)) {
			readQueue1.push({});
			readQueue2.push({});
		}

		simu(wrEn1) = '0';
		simu(wrEn2) = '0';
		for ([[maybe_unused]] auto iter : utils::Range(1000)) {
			co_await AfterClk(clock);

			bool doWrite1 = doWriteDist(rng);
			bool doWrite2 = doWriteDist(rng);
			size_t addrA = addrDist(rng);
			size_t addrB;
			do {
				addrB = addrDist(rng);
			} while (addrA == addrB && (doWrite1 || doWrite2));
			size_t writeData1 = dataDist(rng);
			size_t writeData2 = dataDist(rng);
			size_t writeMask1 = maskDist(rng);
			size_t writeMask2 = maskDist(rng);

			simu(wrEn1) = doWrite1;
			simu(addr1) = addrA;
			simu(wrData1) = writeData1;
			simu(wrMask1) = writeMask1;

			simu(wrEn2) = doWrite2;
			simu(addr2) = addrB;
			simu(wrData2) = writeData2;
			simu(wrMask2) = writeMask2;

			co_await WaitStable();

			auto applyMaskedWrite = [&](size_t a, size_t d, size_t m) {
				sim::DefaultBitVectorState bitMask;
				bitMask.resize(elementWidth.bits());
				for (size_t i = 0; i < maskWidth; i++)
					for (size_t b = 0; b < maskGranularity; b++) {
						bitMask.set(sim::DefaultConfig::DEFINED, i * maskGranularity + b, true);
						bitMask.set(sim::DefaultConfig::VALUE, i * maskGranularity + b, (m >> i) & 1);
					}
				refMemory.write(
					a * elementWidth.bits(),
					sim::parseBitVector(d, elementWidth.bits()),
					false, bitMask);
			};

			if (doWrite1) applyMaskedWrite(addrA, writeData1, writeMask1);
			if (doWrite2) applyMaskedWrite(addrB, writeData2, writeMask2);

			Read r1{};
			if (!doWrite1) {
				r1.active = true;
				r1.readAddr = addrA;
				r1.expected = refMemory.read(addrA * elementWidth.bits(), elementWidth.bits());
			}
			Read r2{};
			if (!doWrite2) {
				r2.active = true;
				r2.readAddr = addrB;
				r2.expected = refMemory.read(addrB * elementWidth.bits(), elementWidth.bits());
			}
			readQueue1.push(r1);
			readQueue2.push(r2);

			auto checkOne = [&](std::queue<Read> &q, UInt &readSig, const char *portName) {
				auto read = std::move(q.front());
				q.pop();
				if (read.active) {
					BOOST_TEST_MESSAGE("In cycle " << iter << " at time " << toDouble(getCurrentSimulationTime()) << "s on " << portName);
					BOOST_TEST_MESSAGE("Read from " << read.readAddr << " expected " << read.expected);
					BOOST_TEST(simu(readSig) == read.expected);
				}
			};
			checkOne(readQueue1, rdValue1, "port1");
			checkOne(readQueue2, rdValue2, "port2");
		}
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		stopTest();
	});

	runTest(Seconds{2000, 1} / clock.absoluteFrequency());
}


void Test_TrueDualPortRMWMaskedWrite::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = ClockConfig::ResetType::NONE,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	size_t maskWidth = elementWidth.bits() / maskGranularity;
	HCL_DESIGNCHECK(elementWidth.bits() % maskGranularity == 0);
	HCL_DESIGNCHECK(elementWidth.bits() <= 32);

	// Port A: always reads at addrA -- this read value feeds into the modify step.
	// Port B: if wrEn, writes (port-A-read + incrData) with mask at addrB; else reads at addrB.
	// Only port B's read is pinned out.
	UInt addrA = pinIn(BitWidth::count(numElements)).setName("addrA");
	UInt addrB = pinIn(BitWidth::count(numElements)).setName("addrB");
	UInt incrData = pinIn(elementWidth).setName("incrData");
	UInt wrMask = pinIn(BitWidth(maskWidth)).setName("wrMask");
	Bit wrEn = pinIn().setName("wrEn");

	Memory<UInt> mem(numElements, elementWidth);
	if (declareNoConflict)
		mem.noConflicts();
	mem.setType(memType, latency);
	mem.initZero();

	// Port A: always reads
	UInt rdValueA = mem[addrA];

	// Port B: write if wrEn, else read
	UInt rdValueB = ConstUInt(elementWidth);
	IF (wrEn)
		mem[addrB] = std::make_pair(rdValueA + incrData, wrMask);
	ELSE
		rdValueB = mem[addrB];

	for ([[maybe_unused]] auto i : utils::Range(latency))
		rdValueB = reg(rdValueB, {.allowRetimingBackward=true});

	pinOut(rdValueB).setName("rdValueB");

	addSimulationProcess([&,this]()->SimProcess {

		std::mt19937 rng(0xC0FFEE);
		std::uniform_int_distribution<size_t> addrDist(0, numElements - 1);
		std::uniform_int_distribution<size_t> dataDist(0, elementWidth.last());
		std::uniform_int_distribution<size_t> maskDist(0, (1ull << maskWidth) - 1);
		std::bernoulli_distribution doWriteDist(0.5f);

		hlim::MemoryStorageDense refMemory(numElements * elementWidth.bits());
		// Mirror mem.initZero(): make refMemory all-defined zero.
		{
			sim::DefaultBitVectorState zero;
			zero.resize(numElements * elementWidth.bits());
			for (size_t i = 0; i < zero.size(); i++) {
				zero.set(sim::DefaultConfig::DEFINED, i, true);
				zero.set(sim::DefaultConfig::VALUE, i, false);
			}
			refMemory.write(0, zero, false, {});
		}

		struct Read {
			bool active = false;
			size_t readAddr = 0;
			sim::DefaultBitVectorState expected;
		};

		std::queue<Read> rdReadQueue;
		for ([[maybe_unused]] auto i : utils::Range(latency))
			rdReadQueue.push({});

		auto stateToUInt = [](const sim::DefaultBitVectorState& s) -> uint64_t {
			return s.extractNonStraddling(sim::DefaultConfig::VALUE, 0, s.size());
		};

		simu(wrEn) = '0';
		simu(addrA) = 0;
		simu(addrB) = 0;
		simu(incrData) = 0;
		simu(wrMask) = 0;

		for ([[maybe_unused]] auto iter : utils::Range(1000)) {
			co_await AfterClk(clock);

			bool doWrite = doWriteDist(rng);
			size_t addrAV = addrDist(rng);
			size_t addrBV;
			// When writing, ensure the two ports don't access the same address simultaneously
			// (cross-port read/write conflict under noConflicts).
			do {
				addrBV = addrDist(rng);
			} while (declareNoConflict && doWrite && addrBV == addrAV);
			size_t incrDataV = dataDist(rng);
			size_t maskV = maskDist(rng);

			simu(wrEn) = doWrite;
			simu(addrA) = addrAV;
			simu(addrB) = addrBV;
			simu(incrData) = incrDataV;
			simu(wrMask) = maskV;

			co_await WaitStable();

			Read rdB{};
			if (!doWrite) {
				rdB.active = true;
				rdB.readAddr = addrBV;
				rdB.expected = refMemory.read(addrBV * elementWidth.bits(), elementWidth.bits());
			}
			rdReadQueue.push(rdB);

			// Apply the write immediately: the actual memory access happens at this cycle;
			// the read latency only delays when results emerge at the output pin.
			if (doWrite) {
				auto rdAValue = refMemory.read(addrAV * elementWidth.bits(), elementWidth.bits());
				uint64_t newVal = stateToUInt(rdAValue) + incrDataV;
				if (elementWidth.bits() < 64)
					newVal &= (1ull << elementWidth.bits()) - 1;
				sim::DefaultBitVectorState bitMask;
				bitMask.resize(elementWidth.bits());
				for (size_t i = 0; i < maskWidth; i++)
					for (size_t b = 0; b < maskGranularity; b++) {
						bitMask.set(sim::DefaultConfig::DEFINED, i * maskGranularity + b, true);
						bitMask.set(sim::DefaultConfig::VALUE, i * maskGranularity + b, (maskV >> i) & 1);
					}
				refMemory.write(
					addrBV * elementWidth.bits(),
					sim::parseBitVector(newVal, elementWidth.bits()),
					false, bitMask);
			}

			auto r = std::move(rdReadQueue.front());
			rdReadQueue.pop();
			if (r.active) {
				BOOST_TEST_MESSAGE("In cycle " << iter << " at time " << toDouble(getCurrentSimulationTime()) << "s on port B");
				BOOST_TEST_MESSAGE("Read from " << r.readAddr << " expected " << r.expected);
				BOOST_TEST(simu(rdValueB) == r.expected);
			}
		}
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		stopTest();
	});

	runTest(Seconds{2000, 1} / clock.absoluteFrequency());
}

