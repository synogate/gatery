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
#include "gatery/pch.h"
#include "Node_MemPort.h"

#include "Node_Memory.h"

#include "../SignalDelay.h"

namespace gtry::hlim {

Node_MemPort::Node_MemPort(std::size_t bitWidth)
{
	resizeInputs((size_t)Inputs::count);
	resizeOutputs((size_t)Outputs::count);
	changeBitWidth(bitWidth);
	setOutputConnectionType((size_t)Outputs::orderBefore, {.type = ConnectionType::ConnectionType::DEPENDENCY, .width=0});
	setOutputConnectionType((size_t)Outputs::memoryWriteDependency, {.type = ConnectionType::ConnectionType::DEPENDENCY, .width=0});
	setOutputType((size_t)Outputs::memoryWriteDependency, OUTPUT_LATCHED);
	m_clocks.resize(1);
}

void Node_MemPort::changeBitWidth(std::size_t bitWidth)
{
	HCL_ASSERT(getDirectlyDriven((size_t)Outputs::rdData).empty());
	HCL_ASSERT(getDriver((size_t)Inputs::address).node == nullptr);
	
	m_bitWidth = bitWidth;
	setOutputConnectionType((size_t)Outputs::rdData, {.type = ConnectionType::BITVEC, .width=bitWidth});
}

void Node_MemPort::connectMemory(Node_Memory *memory)
{
	if (!memory->noConflicts()) {
		Node_MemPort *lastMemPort = memory->getLastPort();
		if (lastMemPort != nullptr)
			orderAfter(lastMemPort);
	}
	connectInput((size_t)Inputs::memoryReadDependency, {.node=memory, .port=(size_t)Node_Memory::Outputs::READ_DEPENDENCIES});
	size_t wrDepInput = memory->createWriteDependencyInputPort();
	memory->rewireInput(wrDepInput, {.node = this, .port = (size_t) Outputs::memoryWriteDependency});
}

void Node_MemPort::disconnectMemory()
{
	auto *memory = getMemory();

	disconnectInput((size_t)Inputs::memoryReadDependency);
	HCL_ASSERT(getDirectlyDriven((size_t) Outputs::memoryWriteDependency).size() == 1);

	auto np = getDirectlyDriven((size_t) Outputs::memoryWriteDependency).front();
	HCL_ASSERT(np.node == memory);

	memory->destroyWriteDependencyInputPort(np.port);
}

size_t Node_MemPort::getExpectedAddressBits() const
{
	size_t depth = (getMemory()->getSize() + m_bitWidth-1) / m_bitWidth;
	size_t expectedBits = utils::Log2C(depth);
	return expectedBits;
}

Node_Memory *Node_MemPort::getMemory() const
{
	return dynamic_cast<Node_Memory *>(getDriver((size_t)Inputs::memoryReadDependency).node);
}

void Node_MemPort::connectEnable(const NodePort &output)
{
	connectInput((size_t)Inputs::enable, output);
}

void Node_MemPort::connectWrEnable(const NodePort &output)
{
	HCL_ASSERT_HINT(!isReadPort(), "For now I don't want to mix read and write ports");
	connectInput((size_t)Inputs::wrEnable, output);
}

void Node_MemPort::connectWrWordEnable(const NodePort &output)
{
	HCL_ASSERT_HINT(!isReadPort(), "For now I don't want to mix read and write ports");
	connectInput((size_t)Inputs::wrWordEnable, output);
}


void Node_MemPort::connectAddress(const NodePort &output)
{
	HCL_ASSERT_HINT(getExpectedAddressBits() == getOutputWidth(output), "Address bus has wrong number of bits!");

	connectInput((size_t)Inputs::address, output);
}

void Node_MemPort::connectWrData(const NodePort &output)
{
	HCL_ASSERT_HINT(!isReadPort(), "For now I don't want to mix read and write ports");
	connectInput((size_t)Inputs::wrData, output);
}

void Node_MemPort::orderAfter(Node_MemPort *writePort)
{
	connectInput((size_t)Inputs::orderAfter, {.node=writePort, .port=(size_t)Node_MemPort::Outputs::orderBefore});
}

bool Node_MemPort::isOrderedAfter(Node_MemPort *port) const
{
	auto *p = (Node_MemPort *)getDriver((size_t)Inputs::orderAfter).node;
	while (p != nullptr) {
		if (p == port) return true;
		p = (Node_MemPort *)p->getDriver((size_t)Inputs::orderAfter).node;
	}
	return false;
}

bool Node_MemPort::isOrderedBefore(Node_MemPort *port) const
{
	auto *p = (Node_MemPort *)port->getDriver((size_t)Inputs::orderAfter).node;
	while (p != nullptr) {
		if (p == this) return true;
		p = (Node_MemPort *)p->getDriver((size_t)Inputs::orderAfter).node;
	}
	return false;
}


void Node_MemPort::setClock(Clock* clk)
{
	attachClock(clk, 0);
}

bool Node_MemPort::isReadPort() const
{
	return !getDirectlyDriven((size_t)Outputs::rdData).empty();
}

bool Node_MemPort::isWritePort() const
{
	return getDriver((size_t)Inputs::wrData).node != nullptr;
}



void Node_MemPort::simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
	// clean all internal stuff
	if (isWritePort()) {
		state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[(size_t)Internal::address], getDriverConnType((size_t)Inputs::address).width);
		state.clearRange(sim::DefaultConfig::DEFINED,internalOffsets[(size_t)Internal::wrData], getBitWidth());
		state.clear(sim::DefaultConfig::VALUE, internalOffsets[(size_t)Internal::wrEnable]);
	}
}

void Node_MemPort::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	if (internalOffsets[(size_t)RefInternal::memory] == ~0ull)
		return;

	const auto &addrType = getDriverConnType((size_t)Inputs::address);

	// optional enables, defaults to true
	bool enableValue = true;
	bool enableDefined = true;
	if (inputOffsets[(size_t)Inputs::enable] != ~0ull) {
		enableValue   = state.get(sim::DefaultConfig::VALUE, inputOffsets[(size_t)Inputs::enable]);
		enableDefined = state.get(sim::DefaultConfig::DEFINED, inputOffsets[(size_t)Inputs::enable]);
	}

	auto prevWPs = getPrevWritePorts();

	// First, do an asynchronous read.
	if (outputOffsets[(size_t)Outputs::rdData] != ~0ull) {

		// If not enabled (or undefined) output undefined since this is an asynchronous read. For synchronous (BRAM) behavior,
		// the register after the read ports holds the read value on a disabled read.
		if (!enableValue || !enableDefined || inputOffsets[(size_t)Inputs::address] == ~0ull) {
			state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[(size_t)Outputs::rdData], getBitWidth());
		} else {
			// Output is undefined if any address bits are undefined.
			// Otherwise the data is read from memory with a (not necessarily pow2) wrap around of the
			// address is larger than the memory.

			std::uint64_t addressValue   = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[(size_t)Inputs::address], addrType.width);
			std::uint64_t addressDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[(size_t)Inputs::address], addrType.width);

			if (!utils::isMaskSet(addressDefined, 0, addrType.width)) {
				state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[(size_t)Outputs::rdData], getBitWidth());
			} else {
				// Fetch value from memory
				auto memSize = getMemory()->getSize();
				HCL_ASSERT(memSize % getBitWidth() == 0);
				auto index = addressValue * getBitWidth();
				if (index >= memSize) {
					state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[(size_t)Outputs::rdData], getBitWidth());
				} else {
					state.copyRange(outputOffsets[(size_t)Outputs::rdData], state, internalOffsets[(size_t)RefInternal::memory] + index, getBitWidth());

					// Check for overrides. Check in order of write ports (they are added last to first).
					// Potentially overwrite what we just read.
					for (size_t i = prevWPs.size()-1; i < prevWPs.size(); i--) {
						// Same order as in  Node_MemPort::getReferencedInternalStateSizes()
						size_t addrOffset = (size_t)RefInternal::prevWritePorts+i*3+0;
						size_t wrDataOffset = (size_t)RefInternal::prevWritePorts+i*3+1;
						size_t wrEnableOffset = (size_t)RefInternal::prevWritePorts+i*3+2;

						const auto &wrAddrType = prevWPs[i]->getDriverConnType((size_t)Inputs::address);
						HCL_ASSERT_HINT(prevWPs[i]->getBitWidth() == getBitWidth(), "Mixed width memory ports not yet supported in simulation!");

						std::uint64_t wrAddressValue   = state.extractNonStraddling(sim::DefaultConfig::VALUE, internalOffsets[addrOffset], wrAddrType.width);
						std::uint64_t wrAddressDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, internalOffsets[addrOffset], wrAddrType.width);

						bool isWriting = state.get(sim::DefaultConfig::VALUE, internalOffsets[wrEnableOffset]);

						if (isWriting) {
							// It's writing something (though write enable might be undefined in which case the stored write data is undefined).
							if (!utils::isMaskSet(wrAddressDefined, 0, wrAddrType.width)) {
								// It's nuking the memory, but since we are dependent, we get a preview.
								state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[(size_t)Outputs::rdData], getBitWidth());
							} else if (wrAddressValue == addressValue) {
								// actual collision, forward to-be-written data
								state.copyRange(outputOffsets[(size_t)Outputs::rdData], state, internalOffsets[wrDataOffset], getBitWidth());
							}
						}
					}
				}
			}
		}
	}

	// If this is also a write port, store all information needed for the write in internal state
	// to perform the write on the clock edge.
	if (isWritePort()) {
		if (inputOffsets[(size_t)Inputs::address] != ~0ull)
			state.copyRange(internalOffsets[(size_t)Internal::address], state, inputOffsets[(size_t)Inputs::address], addrType.width);
		else
			state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[(size_t)Internal::address], addrType.width);

		const auto &wrDataType = getDriverConnType((size_t)Inputs::wrData);
		if (inputOffsets[(size_t)Inputs::wrData] != ~0ull)
			state.copyRange(internalOffsets[(size_t)Internal::wrData], state, inputOffsets[(size_t)Inputs::wrData], wrDataType.width);
		else
			state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[(size_t)Internal::wrData], wrDataType.width);

		bool doWrite = enableValue || !enableDefined;
		bool writingDefined = enableDefined;
		// optional enables, defaults to true
		if (inputOffsets[(size_t)Inputs::wrEnable] != ~0ull) {
			doWrite &= state.get(sim::DefaultConfig::VALUE, inputOffsets[(size_t)Inputs::wrEnable]) ||
						!state.get(sim::DefaultConfig::DEFINED, inputOffsets[(size_t)Inputs::wrEnable]);
			writingDefined &= state.get(sim::DefaultConfig::DEFINED, inputOffsets[(size_t)Inputs::wrEnable]);
		}
		state.set(sim::DefaultConfig::VALUE, internalOffsets[(size_t)Internal::wrEnable], doWrite);

		if (!writingDefined) // If it's unclear whether the write is enabled, do the write but write undefined
			state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[(size_t)Internal::wrData], wrDataType.width);
	}
}

void Node_MemPort::simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
{
	if (internalOffsets[(size_t)RefInternal::memory] == ~0ull)
		return;

	if (isWritePort()) {
		bool doWrite = state.get(sim::DefaultConfig::VALUE, internalOffsets[(size_t)Internal::wrEnable]);

		const auto &addrType = getDriverConnType((size_t)Inputs::address);
		std::uint64_t addressValue   = state.extractNonStraddling(sim::DefaultConfig::VALUE, internalOffsets[(size_t)Internal::address], addrType.width);
		std::uint64_t addressDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, internalOffsets[(size_t)Internal::address], addrType.width);

		if (doWrite) {
			if (!utils::isMaskSet(addressDefined, 0, addrType.width)) {
				// If the address is undefined, make the entire RAM undefined
				state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[(size_t)RefInternal::memory], getMemory()->getSize());
			} else {
				// Perform write, same index computation/behavior as for reads
				auto memSize = getMemory()->getSize();
				HCL_ASSERT(memSize % getBitWidth() == 0);
				auto index = (addressValue * getBitWidth()) % memSize;
				state.copyRange(internalOffsets[(size_t)RefInternal::memory] + index, state, internalOffsets[(size_t)Internal::wrData], getBitWidth());
			}
		}
	}
}




std::string Node_MemPort::getTypeName() const
{
	return "mem_port";
}

void Node_MemPort::assertValidity() const
{
}

std::string Node_MemPort::getInputName(size_t idx) const
{
	switch (idx) {
		case (size_t)Inputs::memoryReadDependency:
			return "memoryReadDependency";
		case (size_t)Inputs::address:
			return "addr";
		case (size_t)Inputs::enable:
			return "enable";
		case (size_t)Inputs::wrEnable:
			return "wrEnable";
		case (size_t)Inputs::wrData:
			return "wrData";
		case (size_t)Inputs::wrWordEnable:
			return "wrWordEnable";
		case (size_t)Inputs::orderAfter:
			return "orderAfter";
		default:
			return "unknown";
	}
}

std::string Node_MemPort::getOutputName(size_t idx) const
{
	switch (idx) {
		case (size_t)Outputs::rdData:
			return "rdData";
		case (size_t)Outputs::orderBefore:
			return "orderBefore";
		case (size_t)Outputs::memoryWriteDependency:
			return "memoryWriteDependency";
		default:
			return "unknown";
	}
}


std::vector<size_t> Node_MemPort::getInternalStateSizes() const
{
	std::vector<size_t> sizes((size_t)Internal::count, 0);
	if (isWritePort()) {

		if (auto driver = getDriver((size_t)Inputs::wrData); driver.node != nullptr)
			sizes[(size_t)Internal::wrData] = driver.node->getOutputConnectionType(driver.port).width;

		sizes[(size_t)Internal::wrEnable] = 1;

		if (auto driver = getDriver((size_t)Inputs::address); driver.node != nullptr)
			sizes[(size_t)Internal::address] = driver.node->getOutputConnectionType(driver.port).width;
	}

	return sizes;
}

std::vector<Node_MemPort*> Node_MemPort::getPrevWritePorts() const
{
	std::vector<Node_MemPort*> res;

	auto *prevNode = (Node_MemPort*)getDriver((size_t)Inputs::orderAfter).node;
	while (prevNode != nullptr) {
		if (prevNode->isWritePort())
			res.push_back(prevNode);
		
		prevNode = (Node_MemPort*)prevNode->getDriver((size_t)Inputs::orderAfter).node;
	}

	return res;
}

std::vector<std::pair<BaseNode*, size_t>> Node_MemPort::getReferencedInternalStateSizes() const
{
	auto prevWPs = getPrevWritePorts();

	std::vector<std::pair<BaseNode*, size_t>> res;
	res.reserve(prevWPs.size()+1);

	res.push_back({getMemory(), (size_t)Node_Memory::Internal::data});
	for (auto wp : prevWPs) {
		res.push_back({wp, (size_t)Internal::address});
		res.push_back({wp, (size_t)Internal::wrData});
		res.push_back({wp, (size_t)Internal::wrEnable});
	}

	return res;
}

std::unique_ptr<BaseNode> Node_MemPort::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_MemPort(m_bitWidth));
	copyBaseToClone(res.get());
	return res;
}

void Node_MemPort::estimateSignalDelay(SignalDelay &sigDelay)
{
	/*
	{
		HCL_ASSERT(sigDelay.contains({.node = this, .port = (size_t)Outputs::orderBefore}));
		auto outDelay = sigDelay.getDelay({.node = this, .port = (size_t)Outputs::orderBefore});
		for (auto &f : outDelay)
			f = 0.0f; 
	}
	*/


	// Ideally, the memory and all it's ports are one cell (e.g. BRAM), so all those signals need to go there
	// So to account for routing delays, count all signals going to this memory, not just those going to this port.
	size_t totalInSignals = 0;
	auto *memory = getMemory();
	for (const auto &np : memory->getPorts()) {
		auto *port = dynamic_cast<Node_MemPort*>(np.node);

		if (port->getDriver((size_t)Inputs::enable).node != nullptr)
			totalInSignals++;

		if (port->getDriver((size_t)Inputs::wrEnable).node != nullptr)
			totalInSignals++;

		if (port->getDriver((size_t)Inputs::address).node != nullptr)
			totalInSignals += getOutputWidth(port->getDriver((size_t)Inputs::address));

		if (port->getDriver((size_t)Inputs::wrData).node != nullptr)
			totalInSignals += getOutputWidth(port->getDriver((size_t)Inputs::wrData));
	}

	{
		auto enableDelay = sigDelay.getDelay(getDriver((size_t)Inputs::enable));
		auto addrDelay = sigDelay.getDelay(getDriver((size_t)Inputs::address));

		HCL_ASSERT(sigDelay.contains({.node = this, .port = (size_t)Outputs::rdData}));
		auto outDelay = sigDelay.getDelay({.node = this, .port = (size_t)Outputs::rdData});

		auto width = getOutputConnectionType((size_t)Outputs::rdData).width;

		float routing = totalInSignals * 0.8f;
		float compute = 6.0f;

		float maxDelay = 0.0f;
		for (auto f : enableDelay)
			maxDelay = std::max(maxDelay, f);
		for (auto f : addrDelay)
			maxDelay = std::max(maxDelay, f);

		for (auto i : utils::Range(width))
			outDelay[i] = maxDelay + routing + compute;
	}
}

void Node_MemPort::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	auto enableDelay = sigDelay.getDelay(getDriver((size_t)Inputs::enable));
	auto addrDelay = sigDelay.getDelay(getDriver((size_t)Inputs::address));

	float maxDelay = 0.0f;
	inputPort = ~0u;
	inputBit = ~0u;
	for (auto i : utils::Range(enableDelay.size())) {
		auto f = enableDelay[i];
		if (f > maxDelay) {
			maxDelay = f;
			inputPort = (size_t)Inputs::enable;
			inputBit = i;
		}
	}

	for (auto i : utils::Range(addrDelay.size())) {
		auto f = addrDelay[i];
		if (f > maxDelay) {
			maxDelay = f;
			inputPort = (size_t)Inputs::address;
			inputBit = i;
		}
	}
}


OutputClockRelation Node_MemPort::getOutputClockRelation(size_t output) const
{
	OutputClockRelation res;

	if (output == (size_t) Outputs::memoryWriteDependency)
		return res;

	res.dependentInputs.reserve(getNumInputPorts()-1);
	for (auto i : utils::Range(getNumInputPorts()))
		if (i != (size_t) Inputs::memoryReadDependency)
			res.dependentInputs.push_back(i);

	// Clock is only for writing, not for reading.

	return res;
}

}
