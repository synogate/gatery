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
#include "gatery/pch.h"

#include "ExternalMemorySimulation.h"

#include "../Circuit.h"
#include "../Clock.h"

#include "../../simulation/simProc/SimulationProcess.h"
#include "../../simulation/simProc/SensitivityList.h"
#include "../../simulation/simProc/WaitChange.h"
#include "../../simulation/simProc/WaitClock.h"
#include "../../simulation/simProc/WaitFor.h"


namespace gtry::hlim {

namespace {



struct WritePortRequest {
	bool enabled = false;
	bool enabledUndefined = false;
	size_t addr = 0;
	bool addrUndefined = false;
	sim::DefaultBitVectorState data;
	sim::DefaultBitVectorState mask;

	void readFromSimulation(const MemorySimConfig::WrPrtNodePorts &port, size_t memorySize) {
		auto addrState = port.addr.eval();
		HCL_ASSERT(addrState.size() > 0);
		addr = addrState.extractNonStraddling(sim::DefaultConfig::VALUE, 0, addrState.size());
		addrUndefined = !sim::allDefinedNonStraddling(addrState, 0, addrState.size());

		if (addr >= memorySize / port.width)
			addrUndefined = true;

		enabledUndefined = false;
		enabled = true;
		if (port.en) {
			auto enabledState = port.en->eval();
			HCL_ASSERT(enabledState.size() == 1);
			enabledUndefined = !enabledState.get(sim::DefaultConfig::DEFINED, 0);
			enabled = enabledState.get(sim::DefaultConfig::VALUE, 0);
		}
		data = port.data.eval();
		if (port.wrMask) {		
			mask = port.wrMask->eval();
			HCL_ASSERT_HINT(mask.size() == data.size(), "Expecting the write mask to be a bit mask with the same width as the written data");
		}
	}
};

struct ReadPortRequest {
	size_t addr = 0;
	bool addrUndefined = false;
	bool readUndefined = false;

	void readFromSimulation(const MemorySimConfig::RdPrtNodePorts &port, size_t memorySize) {
		auto addrState = port.addr.eval();
		HCL_ASSERT(addrState.size() > 0);
		addr = addrState.extractNonStraddling(sim::DefaultConfig::VALUE, 0, addrState.size());
		addrUndefined = !sim::allDefinedNonStraddling(addrState, 0, addrState.size());

		if (addr >= memorySize / port.width)
			addrUndefined = true;

		readUndefined = false;
		if (port.en) {
			auto enabled = port.en->eval();
			HCL_ASSERT(enabled.size() == 1);
			readUndefined |= !enabled.get(sim::DefaultConfig::DEFINED, 0);
			readUndefined |= !enabled.get(sim::DefaultConfig::VALUE, 0);
		}
	}
};

struct ReadPortResponse {
	sim::DefaultBitVectorState data;
};



struct MemoryState {
	// For each port that can write the current write port state.
	std::vector<WritePortRequest> currentWriteRequest;

	std::unique_ptr<MemoryStorage> memory;

	MemoryState(const MemorySimConfig &config) {
		currentWriteRequest.resize(config.writePorts.size());

		if (config.sparse)
			memory = std::make_unique<MemoryStorageSparse>(config.size, config.initialization);
		else
			memory = std::make_unique<MemoryStorageDense>(config.size, config.initialization);
	}
};


bool anyAsync(const MemorySimConfig &config) {
	for (const auto &port : config.readPorts)
		if (port.inputLatency == 0 && port.outputLatency == 0)
			return true;

	return false;
}

sim::SimulationFunction<void> handleReadPortOnce(MemorySimConfig &config, size_t rdPortIdx, MemoryState &memState)
{
	auto &port = config.readPorts[rdPortIdx];

	// Fetch request...
	ReadPortRequest request;
	request.readFromSimulation(port, config.size);

	// Wait for input latency-1 (because sim::WaitClock::DURING of caller will already swallow one cycle)
	if (port.inputLatency > 0) // Can be zero for async ports though
		for ([[maybe_unused]] auto i : utils::Range(port.inputLatency-1))
			co_await sim::WaitClock(port.clk, sim::WaitClock::DURING);

	ReadPortResponse response;
	response.data.resize(port.width);

	size_t rdStart = request.addr * port.width;
	size_t rdEnd = rdStart + port.width;

	if (request.readUndefined || request.addrUndefined)
		response.data.clearRange(sim::DefaultConfig::DEFINED, 0, port.width);
	else {
		// Start with data from memory
		memState.memory->read(response.data, rdStart, port.width);

		// Potentially override with memory writes
		if (port.rdw != MemorySimConfig::RdPrtNodePorts::READ_BEFORE_WRITE) {
			// Check collision

			std::vector<bool> bitsCollided;
			bitsCollided.resize(port.width, false);

			for (auto &wr : memState.currentWriteRequest)
				if (wr.enabled || wr.enabledUndefined) {

					// For undefined write addresses, a collision can not be ruled out, so return undefined.
					// This is a pessimistic simplification, since the written data might equal all the data in the ram.
					if (wr.addrUndefined)
						response.data.clearRange(sim::DefaultConfig::DEFINED, 0, port.width);
					else {

						size_t wrStart = wr.addr * wr.data.size();
						size_t wrEnd = wrStart + wr.data.size();
						bool overlap = (rdStart < wrEnd) && (wrStart < rdEnd);

						if (overlap) {
							// Collision

							// Overlap in memory space
							size_t overlapStart = std::max(rdStart, wrStart);
							size_t overlapEnd = std::min(rdEnd, wrEnd);

							// Overlap in the read word
							size_t wordOverlapStart = overlapStart - rdStart;
							size_t wordOverlapEnd = overlapEnd - rdStart;
							size_t wordOverlapSize = overlapEnd - overlapStart;

							ptrdiff_t read2writeShift = overlapStart - wrStart - wordOverlapStart;

							if (port.rdw == MemorySimConfig::RdPrtNodePorts::READ_UNDEFINED) {
								if (wr.mask.size() != 0) {
									// Don't collide on bits not being written due to write mask
									for (auto i : utils::Range(wordOverlapStart, wordOverlapEnd))
										if (!wr.mask.get(sim::DefaultConfig::DEFINED, i+read2writeShift) || wr.mask.get(sim::DefaultConfig::VALUE, i+read2writeShift))
											response.data.clear(sim::DefaultConfig::DEFINED, i);
								} else
									response.data.clearRange(sim::DefaultConfig::DEFINED, wordOverlapStart, wordOverlapSize);
							} else {
								// MemorySimConfig::RdPrtNodePorts::READ_AFTER_WRITE
								for (auto i : utils::Range(wordOverlapStart, wordOverlapEnd)) {

									// Don't collide on bits not being written due to write mask
									if (wr.mask.size() != 0)
										if (wr.mask.get(sim::DefaultConfig::DEFINED, i+read2writeShift) && !wr.mask.get(sim::DefaultConfig::VALUE, i+read2writeShift))
											continue;

									if (bitsCollided[i]) {
										// Multiple write ports are writing to this location, undefined result (even if same data).
										response.data.clear(sim::DefaultConfig::DEFINED, i);
									} else {
										bitsCollided[i] = true;
										// fetch from write port
										response.data.set(sim::DefaultConfig::VALUE, i, wr.data.get(sim::DefaultConfig::VALUE, i+read2writeShift));
										response.data.set(sim::DefaultConfig::DEFINED, i, wr.data.get(sim::DefaultConfig::DEFINED, i+read2writeShift));
									}
								}
							}
						}
					}
				}
		}
	}

	// Wait for output latency
	for ([[maybe_unused]] auto i : utils::Range(port.outputLatency))
		co_await sim::WaitClock(port.clk, sim::WaitClock::DURING);

	port.data = response.data;
}


void handleAsyncReadPortsOnInputChange(MemorySimConfig &config, MemoryState &memState)
{
	for (auto i : utils::Range(config.readPorts.size())) {
		if (config.readPorts[i].async()) {
			sim::forkFunc(std::function([&config, i, &memState]()->sim::SimulationFunction<void> {
				sim::SensitivityList sensitivityList;
				sensitivityList.add(config.readPorts[i].addr.getOutput());
				if (config.readPorts[i].en)
					sensitivityList.add(config.readPorts[i].en->getOutput());

				while (true) {
					co_await handleReadPortOnce(config, i, memState);
					co_await sim::WaitChange(sensitivityList);
				}
			}));
		}
	}
}

sim::SimulationFunction<void> handleAsyncReadPortsOnce(MemorySimConfig &config, MemoryState &memState)
{
	for (auto i : utils::Range(config.readPorts.size()))
		if (config.readPorts[i].async())
			co_await handleReadPortOnce(config, i, memState);
}


sim::SimulationFunction<void> handleWritePortOnce(MemorySimConfig &config, size_t wrPortIdx, MemoryState &memState)
{
	auto &port = config.writePorts[wrPortIdx];

	WritePortRequest request;
	request.readFromSimulation(port, memState.memory->size());

	for ([[maybe_unused]] auto i : utils::Range(port.inputLatency-1)) // one already swallowed by sim::WaitClock::DURING of caller
		co_await sim::WaitClock(port.clk, sim::WaitClock::DURING);


	// Perform write in this cycle:
	memState.currentWriteRequest[wrPortIdx] = request; 				// declare that we are writing
	co_await handleAsyncReadPortsOnce(config, memState);    // re-trigger async read ports, because write collision may have changed

	if (request.enabled || request.enabledUndefined) {
		// Actually update memory at end of cycle so other read ports can still do read before write in this cycle.
		co_await sim::WaitClock(port.clk, sim::WaitClock::BEFORE);

		if (request.addrUndefined) {
			std::cout << "Warning: Nuking external memory with write enabled (or undefined) and undefined address." << std::endl;
			memState.memory->setAllUndefined();
		} else {
			size_t wrStart = request.addr * port.width;
			size_t wrEnd = wrStart + port.width;

			// Moving forward, clear definedness of request.data for bits that should be set to undefined in memory for whatever reason.
			bool writeAddrCollision = false;
			for (auto j : utils::Range(config.writePorts.size())) {
				if (j == wrPortIdx) continue;
				const auto &otherRequest = memState.currentWriteRequest[j];
				if (otherRequest.enabled || otherRequest.enabledUndefined) {

					if (otherRequest.addrUndefined) {
						writeAddrCollision = true;
						request.data.clearRange(sim::DefaultConfig::DEFINED, 0, port.width);
					} else {
						size_t otherStart = otherRequest.addr * otherRequest.data.size();
						size_t otherEnd = otherStart + otherRequest.data.size();
						bool overlap = (otherStart < wrEnd) && (wrStart < otherEnd);

						if (overlap) {
							writeAddrCollision = true;

							// Overlap in memory space
							size_t overlapStart = std::max(otherStart, wrStart);
							size_t overlapEnd = std::min(otherEnd, wrEnd);

							// Overlap in the write word
							size_t wordOverlapStart = overlapStart - wrStart;
							size_t wordOverlapSize = overlapEnd - overlapStart;

							ptrdiff_t writeShift = overlapStart - otherStart - wordOverlapStart;

							if (otherRequest.mask.size() != 0) {
								// Don't collide on bits not being written due to write mask
								for (auto i : utils::Range(wordOverlapStart, wordOverlapStart + wordOverlapSize))
									if (!otherRequest.mask.get(sim::DefaultConfig::DEFINED, i+writeShift) || otherRequest.mask.get(sim::DefaultConfig::VALUE, i+writeShift))
										request.data.clear(sim::DefaultConfig::DEFINED, i);
							} else
								request.data.clearRange(sim::DefaultConfig::DEFINED, wordOverlapStart, wordOverlapSize);
						}
					}
				}
			}

			if (writeAddrCollision)
				std::cout << "Warning: Two write ports are trying to write to the same memory location." << std::endl;

			// Actually perform the write to memory, which might be partially or fully undefined by now
			memState.memory->write(wrStart, request.data, request.enabledUndefined, request.mask);
		}
	}
}

}


void addExternalMemorySimulator(Circuit &circuit, MemorySimConfig config)
{
	for (auto &port : config.writePorts)
		HCL_ASSERT_HINT(port.inputLatency > 0, "Write ports must be synchronous (have an input latency > 0).");


	HCL_ASSERT_HINT(!config.warnRWCollision, "Not yet implemented");
	HCL_ASSERT_HINT(!config.warnUncontrolledWrite, "Not yet implemented");


	// The simulation of ports must work within the reset to accommodate simulation of reset logic.
	// To this end, find or create a reset-free clock for every clock used in this memory.

	utils::UnstableMap<Clock*, Clock*> clock2resetFreeClk;
	auto switchToResetClock = [&clock2resetFreeClk, &circuit](Clock *&clk) {
		if (clk != nullptr) {
			auto it = clock2resetFreeClk.find(clk);
			if (it == clock2resetFreeClk.end()) {
				Clock *resetFreeSimulationClock = circuit.createClock<DerivedClock>(clk);
				resetFreeSimulationClock->setName("reset_free_clock_for_memory_simulation");
				resetFreeSimulationClock->getRegAttribs().resetType = RegisterAttributes::ResetType::NONE;
				resetFreeSimulationClock->getRegAttribs().memoryResetType = RegisterAttributes::ResetType::NONE;
				
				clock2resetFreeClk.emplace(clk, resetFreeSimulationClock);
				clk = resetFreeSimulationClock;
			} else {
				clk = it->second;
			}
		}
	};

	for (auto &port : config.readPorts)
		switchToResetClock(port.clk);

	for (auto &port : config.writePorts)
		switchToResetClock(port.clk);

	// Build the main simulation process.
	// State resources (memory) must be created within to assure lifetime.
	// Config must be copied in for the same reason.

	circuit.addSimulationProcess([config]()mutable->sim::SimulationFunction<>{

		MemoryState memState(config);

		// Start write ports before read ports so that currentWriteRequest is always up to date for the reads
		for (auto i : utils::Range(config.writePorts.size())) {
			sim::forkFunc(std::function([&config, i, &memState]()->sim::SimulationFunction<void> {
				auto &port = config.writePorts[i];
				while (true) {
					co_await sim::WaitClock(port.clk, sim::WaitClock::DURING);
					sim::forkFunc(handleWritePortOnce(config, i, memState));
				}
			}));
		}

		// Init and start read ports
		for (auto i : utils::Range(config.readPorts.size())) {
			config.readPorts[i].data.invalidate();
			
			// Run sync read ports every cycle
			if (!config.readPorts[i].async())
				sim::forkFunc(std::function([&config, i, &memState]()->sim::SimulationFunction<void> {
					while (true) {
						co_await sim::WaitClock(config.readPorts[i].clk, sim::WaitClock::DURING);
						sim::forkFunc(handleReadPortOnce(config, i, memState));
					}
				}));
		}

		// Start async read ports
		handleAsyncReadPortsOnInputChange(config, memState);

		// Wait forever to keep local variables (memory state) alive
		while (true) co_await sim::WaitFor({1,1});
	});


}


}