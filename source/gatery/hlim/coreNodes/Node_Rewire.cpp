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
#include "Node_Rewire.h"
#include "Node_Constant.h"

#include "../SignalDelay.h"

#include "../../debug/DebugInterface.h"

namespace gtry::hlim {

bool Node_Rewire::RewireOperation::isBitExtract(size_t& bitIndex) const
{
	if (ranges.size() == 1) {
		if (ranges[0].subwidth == 1 &&
			ranges[0].source == OutputRange::INPUT &&
			ranges[0].inputIdx == 0) {

			bitIndex = ranges[0].inputOffset;
			return true;
		}
	}
	return false;
}

Node_Rewire::RewireOperation& Node_Rewire::RewireOperation::addInput(size_t inputIndex, size_t inputOffset, size_t width)
{
	if (width > 0)
	{
		ranges.emplace_back(OutputRange{
			.subwidth = width,
			.source = OutputRange::INPUT,
			.inputIdx = inputIndex,
			.inputOffset = inputOffset
			});
	}
	return *this;
}

Node_Rewire::RewireOperation& Node_Rewire::RewireOperation::addConstant(OutputRange::Source type, size_t width)
{
	HCL_ASSERT(type != OutputRange::INPUT);

	if (width > 0)
	{
		ranges.emplace_back(OutputRange{
			.subwidth = width,
			.source = type,
			.inputIdx = 0,
			.inputOffset = 0
			});
	}
	return *this;
}

Node_Rewire::Node_Rewire(size_t numInputs) : Node(numInputs, 1)
{
}

void Node_Rewire::connectInput(size_t operand, const NodePort &port)
{
	NodeIO::connectInput(operand, port);
	updateConnectionType();
}

void Node_Rewire::setConcat()
{
	RewireOperation op;
	op.ranges.reserve(getNumInputPorts());

	for (size_t i = 0; i < getNumInputPorts(); ++i)
	{
		NodePort driver = getDriver(i);
		HCL_ASSERT(driver.node);
		size_t width = driver.node->getOutputConnectionType(driver.port).width;

		op.ranges.emplace_back(OutputRange{
			.subwidth = width,
			.source = OutputRange::INPUT,
			.inputIdx = i,
			.inputOffset = 0
		});
	}
	setOp(std::move(op));
}

void Node_Rewire::setExtract(size_t offset, size_t count)
{
	HCL_DESIGNCHECK(getNumInputPorts() == 1);

	size_t inWidth = getDriverConnType(0).width;

	RewireOperation op;
	if(offset < inWidth)
		op.addInput(0, offset, std::min(offset + count, inWidth) - offset);

	if(offset + count > inWidth)
		op.addConstant(OutputRange::CONST_UNDEFINED, std::min(offset + count - inWidth, count));

	setOp(std::move(op));
}

void Node_Rewire::setReplaceRange(size_t offset)
{
	HCL_ASSERT(getNumInputPorts() == 2);

	const ConnectionType type0 = getDriverConnType(0);
	const ConnectionType type1 = getDriverConnType(1);
	HCL_ASSERT(type0.width >= type1.width + offset);

	hlim::Node_Rewire::RewireOperation op;
	op.addInput(0, 0, offset);
	op.addInput(1, 0, type1.width);
	op.addInput(0, offset + type1.width, type0.width - (offset + type1.width));

	setOp(std::move(op));
}

void Node_Rewire::setPadTo(size_t width, OutputRange::Source padding)
{
	HCL_ASSERT(getNumInputPorts() == 1);

	const ConnectionType type0 = getDriverConnType(0);

	hlim::Node_Rewire::RewireOperation op;
	op.addInput(0, 0, std::min(width, type0.width));
	if(width > type0.width)
		op.addConstant(padding, width - type0.width);

	setOp(std::move(op));
}

void Node_Rewire::setPadTo(size_t width)
{
	HCL_ASSERT(getNumInputPorts() == 1);

	const ConnectionType type0 = getDriverConnType(0);
	HCL_DESIGNCHECK(type0.width > 0);

	hlim::Node_Rewire::RewireOperation op;
	op.addInput(0, 0, std::min(width, type0.width));
	for(size_t i = type0.width; i < width; ++i)
		op.addInput(0, type0.width-1, 1);

	setOp(std::move(op));
}

void Node_Rewire::changeOutputType(ConnectionType outputType)
{
	m_desiredConnectionType = outputType;
	updateConnectionType();
}

void Node_Rewire::updateConnectionType()
{
	ConnectionType desiredConnectionType = m_desiredConnectionType;

	desiredConnectionType.width = 0;
	for (auto r : m_rewireOperation.ranges)
		desiredConnectionType.width += r.subwidth;
	//HCL_ASSERT(desiredConnectionType.width <= 64);

	setOutputConnectionType(0, desiredConnectionType);
}


void Node_Rewire::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	//HCL_ASSERT_HINT(getOutputConnectionType(0).width <= 64, "Rewiring with more than 64 bits not yet implemented!");

	size_t totalWidth = getOutputConnectionType(0).width;
	if (totalWidth <= 64) {
		// fast path

		std::uint64_t resValue = 0;
		std::uint64_t resDefined = 0;

		std::uint64_t v;
		std::uint64_t d;

		size_t outputOffset = 0;
		for (auto rangeIdx : utils::Range(m_rewireOperation.ranges.size())) {
			const auto &range = m_rewireOperation.ranges[rangeIdx];

			if (range.source == OutputRange::INPUT) {
				if (inputOffsets[range.inputIdx] != ~0ull) {
					if (range.subwidth == 1) {
						if (rangeIdx > 0 && m_rewireOperation.ranges[rangeIdx-1] == range) {
							// reuse last value, this can happen quite often for sign bit extension
						} else {
							v = state.get(sim::DefaultConfig::VALUE, inputOffsets[range.inputIdx]+range.inputOffset)?1:0;
							d = state.get(sim::DefaultConfig::DEFINED, inputOffsets[range.inputIdx]+range.inputOffset)?1:0;
						}
					} else {
						v = state.extract(sim::DefaultConfig::VALUE, inputOffsets[range.inputIdx]+range.inputOffset, range.subwidth);
						d = state.extract(sim::DefaultConfig::DEFINED, inputOffsets[range.inputIdx]+range.inputOffset, range.subwidth);
					}
					resValue |= v << outputOffset;
					resDefined |= d << outputOffset;
				}
			} else {
				uint64_t dstMask = utils::bitMaskRange(outputOffset, range.subwidth);
				switch (range.source) {
					case OutputRange::CONST_ONE:
						resDefined |= dstMask;
						resValue |= dstMask;
					break;
					case OutputRange::CONST_ZERO:
						resDefined |= dstMask;
					break;
					case OutputRange::CONST_UNDEFINED:
					break;
					case OutputRange::INPUT:
						// Impossible, but here to prevent warning
					break;
				}
			}
			outputOffset += range.subwidth;
		}

		state.insert(sim::DefaultConfig::VALUE, outputOffsets[0], totalWidth, resValue);
		state.insert(sim::DefaultConfig::DEFINED, outputOffsets[0], totalWidth, resDefined);

	} else {
		size_t outputOffset = 0;
		for (const auto &range : m_rewireOperation.ranges) {
			switch (range.source) {
				case OutputRange::CONST_ONE:
					state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0] + outputOffset, range.subwidth);
					state.setRange(sim::DefaultConfig::VALUE, outputOffsets[0] + outputOffset, range.subwidth);
				break;
				case OutputRange::CONST_ZERO:
					state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0] + outputOffset, range.subwidth);
					state.clearRange(sim::DefaultConfig::VALUE, outputOffsets[0] + outputOffset, range.subwidth);
				break;
				case OutputRange::CONST_UNDEFINED:
					state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0] + outputOffset, range.subwidth);
				break;
				case OutputRange::INPUT:
					if (inputOffsets[range.inputIdx] == ~0ull)
						state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0] + outputOffset, range.subwidth);
					else
						state.copyRange(outputOffsets[0] + outputOffset, state, inputOffsets[range.inputIdx]+range.inputOffset, range.subwidth);
				break;
			}
			outputOffset += range.subwidth;
		}
	}
}

std::string Node_Rewire::getTypeName() const
{
/*
	size_t bitIndex;
	if (m_rewireOperation.isBitExtract(bitIndex))
		return std::string("bit ") + std::to_string(bitIndex);
	else
	*/
		return "Rewire";
}

bool Node_Rewire::isNoOp() const
{
	if (getNumInputPorts() == 0) return false;
	if (getDriver(0).node == nullptr) return false;
	if (getOutputConnectionType(0) != getDriverConnType(0)) return false;

	auto outWidth = getOutputConnectionType(0).width;

	size_t offset = 0;
	for (const auto &range : m_rewireOperation.ranges) {
		if (range.source != OutputRange::INPUT) return false;
		if (range.inputIdx != 0) return false;
		if (range.inputOffset != offset) return false;
		offset += range.subwidth;
	}
	HCL_ASSERT(offset == outWidth);

	if (offset != getDriverConnType(0).width) return false;

	return true;
}

bool Node_Rewire::bypassIfNoOp()
{
	if (isNoOp()) {
		bypassOutputToInput(0, 0);
		return true;
	}
	return false;
}



std::unique_ptr<BaseNode> Node_Rewire::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_Rewire(getNumInputPorts()));
	copyBaseToClone(res.get());
	((Node_Rewire*)res.get())->m_desiredConnectionType = m_desiredConnectionType;
	((Node_Rewire*)res.get())->m_rewireOperation = m_rewireOperation;
	return res;
}


std::string Node_Rewire::attemptInferOutputName(size_t outputPort) const
{
	if (getNumInputPorts() == 0)
		return "concatenated_constants";

	size_t bitIndex;
	if (m_rewireOperation.isBitExtract(bitIndex)) {
		auto driver0 = getDriver(0);
		if (driver0.node == nullptr) return "";
		if (inputIsComingThroughParentNodeGroup(0)) return "";
		if (driver0.node->getName().empty()) return "";

		std::stringstream name;
		name << driver0.node->getName() << "_bit_" << bitIndex;
		return name.str();
	} else {
		if (getNumInputPorts() == 0)
			return "rewired_constants";
		std::stringstream name;
		bool first = true;
		for (auto i : utils::Range(getNumInputPorts())) {
			auto driver = getDriver(i);
			if (driver.node == nullptr)
				return "";
			if (driver.node->getOutputConnectionType(driver.port).isDependency()) continue;
			if (inputIsComingThroughParentNodeGroup(i)) return "";
			if (driver.node->getName().empty()) {
				return "";
			} else {
				if (!first) name << '_';
				first = false;
				name << driver.node->getName();
			}
		}
		name << "_rewired";
		return name.str();
	}
}



void Node_Rewire::estimateSignalDelay(SignalDelay &sigDelay)
{
	std::vector<std::span<float>> inDelays;
	inDelays.resize(getNumInputPorts());
	for (auto i : utils::Range(getNumInputPorts()))
		inDelays[i] = sigDelay.getDelay(getDriver(i));

	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});

	//auto width = getOutputConnectionType(0).width;

	size_t outputOffset = 0;
	for (const auto &range : m_rewireOperation.ranges) {
		if (range.source == OutputRange::INPUT) {
			for (auto i : utils::Range(range.subwidth))
				outDelay[outputOffset+i] = inDelays[range.inputIdx][range.inputOffset+i];
		} else {
			for (auto i : utils::Range(range.subwidth))
				outDelay[outputOffset+i] = 0.0f;
		}
		outputOffset += range.subwidth;
	}	
}

void Node_Rewire::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	size_t outputOffset = 0;
	for (const auto &range : m_rewireOperation.ranges) {
		if (outputBit >= outputOffset && outputBit < outputOffset+range.subwidth) {
			if (range.source == OutputRange::INPUT) {
				inputPort = range.inputIdx;
				inputBit = outputBit - outputOffset + range.inputOffset;
			} else {
				inputPort = ~0u;
				inputBit = ~0u;
			}
			return;
		}
		outputOffset += range.subwidth;
	}	

	HCL_ASSERT(false);
}

void Node_Rewire::optimize()
{
	bool zeroWidthSubrangesRemoved = false;
	bool mergedSubranges = false;
	bool constantsWereMerged = false;
	bool inputsWereMerged = false;
	bool inputsWereRemoved = false;
	auto op = m_rewireOperation;

	// remove zero subwidth ranges

	for (size_t i = 0; i < op.ranges.size(); i++) {
		if (op.ranges[i].subwidth == 0) {
			zeroWidthSubrangesRemoved = true;
			op.ranges.erase(op.ranges.begin()+i); 
			i--;
		}
	}

	// Merge const zero and const one inputs

	for (size_t inputIdx : utils::Range(getNumInputPorts())) {
		if (auto *constant = dynamic_cast<Node_Constant*>(getNonSignalDriver(inputIdx).node)) {
			if (sim::allDefined(constant->getValue())) {
				bool allZero = sim::allZero(constant->getValue(), sim::DefaultConfig::VALUE);
				bool allOne = sim::allOne(constant->getValue(), sim::DefaultConfig::VALUE);

				if (allZero || allOne)
					for (auto &r : op.ranges)
						if (r.source == Node_Rewire::OutputRange::INPUT && r.inputIdx == inputIdx) {
							r.source = allZero?Node_Rewire::OutputRange::CONST_ZERO:Node_Rewire::OutputRange::CONST_ONE;
							constantsWereMerged = true;
						}
			}
		}
	}

	// merge ranges
	for (size_t i = 0; i+1 < op.ranges.size(); i++) {
		if ((op.ranges[i].source == Node_Rewire::OutputRange::CONST_ZERO && 
		 	 op.ranges[i+1].source == Node_Rewire::OutputRange::CONST_ZERO) ||
			(op.ranges[i].source == Node_Rewire::OutputRange::CONST_ONE && 
		 	 op.ranges[i+1].source == Node_Rewire::OutputRange::CONST_ONE) ||
			(op.ranges[i].source == Node_Rewire::OutputRange::INPUT && 
			 op.ranges[i+1].source == Node_Rewire::OutputRange::INPUT && 
			 op.ranges[i].inputIdx == op.ranges[i+1].inputIdx &&
			 op.ranges[i+1].inputOffset == op.ranges[i].inputOffset + op.ranges[i].subwidth)) {

			op.ranges[i].subwidth += op.ranges[i+1].subwidth;
			op.ranges.erase(op.ranges.begin()+i+1);
			mergedSubranges = true;
		}
	}

	// Deduplicate inputs, remove unused inputs
	utils::StableMap<NodePort, size_t> remappedInputs;
	for (auto &r : op.ranges)
		if (r.source == Node_Rewire::OutputRange::INPUT) {
			auto driver = getDriver(r.inputIdx);
			size_t idx;
			auto it = remappedInputs.find(driver);
			if (it == remappedInputs.end()) {
				idx = remappedInputs.size();
				remappedInputs[driver] = idx;
			} else {
				inputsWereMerged = true;
				idx = it->second;
			}
			r.inputIdx = idx;
		}
	inputsWereRemoved = remappedInputs.size() != getNumInputPorts();

	resizeInputs(remappedInputs.size());
	for (const auto &inp : remappedInputs)
		connectInput(inp.second, inp.first);

	setOp(std::move(op));

	if (zeroWidthSubrangesRemoved)
		dbg::log(dbg::LogMessage(getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "In rewire node " << this << " removed zero subwidth ranges." );

	if (constantsWereMerged)
		dbg::log(dbg::LogMessage(getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "In rewire node " << this << " merged explicit constant inputs into rewire operation." );

	if (mergedSubranges)
		dbg::log(dbg::LogMessage(getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "In rewire node " << this << " merged consecutive, compatible ranges." );

	if (inputsWereMerged)
		dbg::log(dbg::LogMessage(getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "In rewire node " << this << " merged redundant inputs." );

	if (inputsWereRemoved)
		dbg::log(dbg::LogMessage(getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "In rewire node " << this << " removed unused or redundant inputs." );
}

bool Node_Rewire::outputIsConstant(size_t port) const
{
	for (const auto &range : m_rewireOperation.ranges) {
		if (range.subwidth == 0) continue;

		if (range.source == OutputRange::Source::INPUT)
			return false;
	}
	return true;
}

}
