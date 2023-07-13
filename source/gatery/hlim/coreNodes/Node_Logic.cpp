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
#include "Node_Logic.h"
#include "Node_Constant.h"

#include "../SignalDelay.h"


namespace gtry::hlim {

Node_Logic::Node_Logic(Op op) : Node(op==NOT?1:2, 1), m_op(op)
{

}

void Node_Logic::connectInput(size_t operand, const NodePort &port)
{
	NodeIO::connectInput(operand, port);
	updateConnectionType();
}

void Node_Logic::disconnectInput(size_t operand)
{
	NodeIO::disconnectInput(operand);
}

void Node_Logic::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	size_t width = getOutputConnectionType(0).width;

	NodePort leftDriver = getDriver(0);
	bool leftAllUndefined = leftDriver.node == nullptr;
	/*
	if (leftDriver.node == nullptr) {
		state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
		return;
	}
	*/

	NodePort rightDriver;
	bool rightAllUndefined = true;
	if (m_op != NOT) {
		rightDriver = getDriver(1);
		rightAllUndefined = rightDriver.node == nullptr;
	}

	size_t offset = 0;

	while (offset < width) {
		size_t chunkSize = std::min<size_t>(64, width-offset);

		std::uint64_t left, leftDefined, right, rightDefined;

		if (leftAllUndefined || inputOffsets[0] == ~0ull) {
			leftDefined = 0;
			left = 0;
		} else {
			leftDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[0]+offset, chunkSize);
			left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0]+offset, chunkSize);
		}

		if (rightAllUndefined || m_op == NOT || inputOffsets[1] == ~0ull) {
			rightDefined = 0;
			right = 0;
		} else {
			rightDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[1]+offset, chunkSize);
			right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1]+offset, chunkSize);
		}

		std::uint64_t result, resultDefined;
		switch (m_op) {
			case AND:
				result = left & right;
				resultDefined = (leftDefined & ~left) | (rightDefined & ~right) | (leftDefined & rightDefined);
			break;
			case NAND:
				result = ~(left & right);
				resultDefined = (leftDefined & ~left) | (rightDefined & ~right) | (leftDefined & rightDefined);
			break;
			case OR:
				result = left | right;
				resultDefined = (leftDefined & left) | (rightDefined & right) | (leftDefined & rightDefined);
			break;
			case NOR:
				result = ~(left | right);
				resultDefined = (leftDefined & left) | (rightDefined & right) | (leftDefined & rightDefined);
			break;
			case XOR:
				result = left ^ right;
				resultDefined = leftDefined & rightDefined;
			break;
			case EQ:
				result = ~(left ^ right);
				resultDefined = leftDefined & rightDefined;
			break;
			case NOT:
				result = ~left;
				resultDefined = leftDefined;
			break;
		};

		state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0] + offset, chunkSize, result);
		state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0] + offset, chunkSize, resultDefined);
		offset += chunkSize;
	}
}

bool Node_Logic::checkIsNoOp(size_t &inputToForward) const
{
	if (m_op == NOT) {
		// For not, it is only a no-op if the input is constant undefined.
		if (auto *constNode = dynamic_cast<Node_Constant*>(getNonSignalDriver(0).node)) {
			if (sim::anyDefined(constNode->getValue()))
				return false;
			else {
				inputToForward = 0;
				return true;
			}
		} else
			return false;
	}

	// For all binary operations, check if one of the inputs is a constant
	Node_Constant *constant = nullptr;
	size_t constIndex;
	size_t otherDriverIndex;
	if (auto *constNode = dynamic_cast<Node_Constant*>(getNonSignalDriver(0).node)) {
		constant = constNode;
		constIndex = 0;
		otherDriverIndex = 1;
	} else 	if (auto *constNode = dynamic_cast<Node_Constant*>(getNonSignalDriver(1).node)) {
		constant = constNode;
		constIndex = 1;
		otherDriverIndex = 0;
	} 

	if (constant == nullptr)
		return false;


	// Check if all bits are either one or zero
	if (!sim::allDefined(constant->getValue()))
		return false;
	
	bool allOne = true;
	bool allZero = true;

	for (auto i : utils::Range(getOutputConnectionType(0).width)) {
		bool bit = constant->getValue().get(sim::DefaultConfig::VALUE, i);
		allOne &= bit;
		allZero &= !bit;
	}

	if (!allOne && !allZero)
		return false;


	// Decide which to forward
	switch (m_op) {
		case AND:
			if (allOne)
				inputToForward = otherDriverIndex; // AND with '1' means we can forward the other driver
			if (allZero)
				inputToForward = constIndex; // AND with '0' means we can forward the '0' driver
		break;
		case OR:
			if (allOne)
				inputToForward = constIndex; // OR with '1' means we can forward the '1' driver
			if (allZero)
				inputToForward = otherDriverIndex; // OR with '0' means we can forward the other driver
		break;
		default:
			return false;
	};
	return true;
}

bool Node_Logic::isNoOp() const
{
	size_t unused;
	return checkIsNoOp(unused);
}

bool Node_Logic::bypassIfNoOp()
{
	size_t inputIdx;
	if (checkIsNoOp(inputIdx)) {
		bypassOutputToInput(0, inputIdx);
		return true;
	}
	return false;
}


std::string Node_Logic::getTypeName() const
{
	switch (m_op) {
		case AND: return "and";
		case NAND: return "nand";
		case OR: return "or";
		case NOR: return "nor";
		case XOR: return "xor";
		case EQ: return "bitwise-equal";
		case NOT: return "not";
		default:
			return "Logic";
	}
}

void Node_Logic::assertValidity() const
{

}

std::string Node_Logic::getInputName(size_t idx) const
{
	return idx==0?"a":"b";
}

std::string Node_Logic::getOutputName(size_t idx) const
{
	return "output";
}


void Node_Logic::updateConnectionType()
{
	auto lhs = getDriver(0);
	NodePort rhs;
	if (m_op != NOT)
		rhs = getDriver(1);

	ConnectionType desiredConnectionType = getOutputConnectionType(0);

	if (lhs.node != nullptr) {
		if (rhs.node != nullptr) {
			desiredConnectionType = hlim::getOutputConnectionType(lhs);
			HCL_ASSERT_HINT(desiredConnectionType == hlim::getOutputConnectionType(rhs), "Support for differing types of input to logic node not yet implemented");
			//desiredConnectionType.width = std::max(desiredConnectionType.width, rhs.node->getOutputConnectionType(rhs.port).width);
		} else
			desiredConnectionType = hlim::getOutputConnectionType(lhs);
	} else if (rhs.node != nullptr)
		desiredConnectionType = hlim::getOutputConnectionType(rhs);

	setOutputConnectionType(0, desiredConnectionType);
}


std::unique_ptr<BaseNode> Node_Logic::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_Logic(m_op));
	copyBaseToClone(res.get());
	return res;
}

std::string Node_Logic::attemptInferOutputName(size_t outputPort) const
{
	std::stringstream name;

	auto driver0 = getDriver(0);
	if (driver0.node == nullptr) return "";
	if (inputIsComingThroughParentNodeGroup(0)) return "";
	if (driver0.node->getName().empty()) return "";

	name << driver0.node->getName();

	switch (m_op) {
		case AND: name << "_and_"; break;
		case NAND: name << "_nand_"; break;
		case OR: name << "_or_"; break;
		case NOR: name << "_nor_"; break;
		case XOR: name << "_xor_"; break;
		case EQ: name << "_bweq_"; break;
		case NOT: name << "_not"; break;
		default: name << "_logic_"; break;
	}

	if (getNumInputPorts() > 1) {
		auto driver1 = getDriver(1);
		if (driver1.node == nullptr) return "";
		if (inputIsComingThroughParentNodeGroup(1)) return "";
		if (driver1.node->getName().empty()) return "";

		name << driver1.node->getName();
	}
	return name.str();
}


void Node_Logic::estimateSignalDelay(SignalDelay &sigDelay)
{
	auto inDelay0 = sigDelay.getDelay(getDriver(0));

	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});

	auto width = getOutputConnectionType(0).width;

	if (m_op == NOT) {
		for (auto i : utils::Range(width))
			outDelay[i] = inDelay0[i] + 0.1f;
	} else {
		auto inDelay1 = sigDelay.getDelay(getDriver(1));

		float routing = 2 * 0.8f;
		float compute = 0.1f;

		for (auto i : utils::Range(width))
			outDelay[i] = std::max(inDelay0[i], inDelay1[i]) + routing + compute;
	}
}

void Node_Logic::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputBit = outputBit;
	if (m_op == NOT) {
		inputPort = 0;
	} else {
		auto inDelay0 = sigDelay.getDelay(getDriver(0));
		auto inDelay1 = sigDelay.getDelay(getDriver(1));

		if (inDelay0[outputBit] > inDelay1[outputBit]) 
			inputPort = 0;
		else
			inputPort = 1;
	}
}


}
