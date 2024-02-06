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
#include "Node_Compare.h"

#include <gatery/simulation/BitVectorState.h>

#include "../SignalDelay.h"


namespace gtry::hlim {

Node_Compare::Node_Compare(Op op) : Node(2, 1), m_op(op)
{
	ConnectionType conType;
	conType.width = 1;
	conType.type = ConnectionType::BOOL;
	setOutputConnectionType(0, conType);
}

bool Node_Compare::outputIsConstant(size_t) const
{
	auto leftDriver = getDriver(0);
	auto rightDriver = getDriver(1);

	if (leftDriver.node== nullptr || rightDriver.node == nullptr)
		return false;

	const auto &leftType = hlim::getOutputConnectionType(leftDriver);
	const auto &rightType = hlim::getOutputConnectionType(rightDriver);
	HCL_ASSERT_HINT(leftType.type == rightType.type, "Comparing signals with different interpretations not yet implemented!");

	// Special handling for zero-width inputs
	return leftType.width == 0 && rightType.width == 0;
}

void Node_Compare::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	auto leftDriver = getDriver(0);
	auto rightDriver = getDriver(1);

	if (leftDriver.node== nullptr || rightDriver.node == nullptr) {
		state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
		return;
	}	

	const auto &leftType = hlim::getOutputConnectionType(leftDriver);
	const auto &rightType = hlim::getOutputConnectionType(rightDriver);
	HCL_ASSERT_HINT(leftType.type == rightType.type, "Comparing signals with different interpretations not yet implemented!");

	// Special handling for zero-width inputs
	if (leftType.width == 0 && rightType.width == 0) {
		bool result;
		switch (m_op) {
			case EQ:
				result = true;
			break;
			case NEQ:
				result = false;
			break;
			case LT:
				result = false;
			break;
			case GT:
				result = false;
			break;
			case LEQ:
				result = true;
			break;
			case GEQ:
				result = true;
			break;
			default:
				HCL_ASSERT_HINT(false, "Unhandled case!");
		}
		state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, result?1:0);
		state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0], 1, 1);
		return;
	}

	if (inputOffsets[0] == ~0ull || inputOffsets[1] == ~0ull ||
		!allDefined(state, inputOffsets[0], leftType.width) || !allDefined(state, inputOffsets[1], rightType.width)) {

		state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
		return;
	}

	bool result;
	if (leftType.width <= 64 && rightType.width <= 64) {
		std::uint64_t left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], leftType.width);
		std::uint64_t right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1], rightType.width);

		switch (leftType.type) {
			case ConnectionType::BOOL:
				switch (m_op) {
					case EQ:
						result = left == right;
					break;
					case NEQ:
						result = left != right;
					break;
					default:
						HCL_ASSERT_HINT(false, "Unhandled case!");
				}
			break;
			case ConnectionType::BITVEC:
				switch (m_op) {
					case EQ:
						result = left == right;
					break;
					case NEQ:
						result = left != right;
					break;
					case LT:
						result = left < right;
					break;
					case GT:
						result = left > right;
					break;
					case LEQ:
						result = left <= right;
					break;
					case GEQ:
						result = left >= right;
					break;
					default:
						HCL_ASSERT_HINT(false, "Unhandled case!");
				}
			break;
			default:
				HCL_ASSERT_HINT(false, "Unhandled case!");
		}
	} else {
		sim::BigInt left, right;

		left = sim::extractBigInt(state, inputOffsets[0], leftType.width);
		right = sim::extractBigInt(state, inputOffsets[1], rightType.width);

		HCL_ASSERT(leftType.isBitVec());
		switch (m_op) {
			case EQ:
				result = left == right;
			break;
			case NEQ:
				result = left != right;
			break;
			case LT:
				result = left < right;
			break;
			case GT:
				result = left > right;
			break;
			case LEQ:
				result = left <= right;
			break;
			case GEQ:
				result = left >= right;
			break;
			default:
				HCL_ASSERT_HINT(false, "Unhandled case!");
		}
	}
	state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, result?1:0);
	state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0], 1, 1);
}


std::string Node_Compare::getTypeName() const
{
	switch (m_op) {
		case EQ:  return "==";
		case NEQ: return "!=";
		case LT:  return "<";
		case GT:  return ">";
		case LEQ: return "<=";
		case GEQ: return ">=";
		default:  return "Compare";
	}
}

void Node_Compare::assertValidity() const
{

}

std::string Node_Compare::getInputName(size_t idx) const
{
	return idx==0?"a":"b";
}

std::string Node_Compare::getOutputName(size_t idx) const
{
	return "out";
}

std::unique_ptr<BaseNode> Node_Compare::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_Compare(m_op));
	copyBaseToClone(res.get());
	return res;
}

std::string Node_Compare::attemptInferOutputName(size_t outputPort) const
{
	std::stringstream name;

	auto driver0 = getDriver(0);
	if (driver0.node == nullptr) return "";
	if (inputIsComingThroughParentNodeGroup(0)) return "";
	if (driver0.node->getName().empty()) return "";

	name << driver0.node->getName();

	switch (m_op) {
		case EQ:  name << "_eq_"; break;
		case NEQ: name << "_neq_"; break;
		case LT:  name << "_lt_"; break;
		case GT:  name << "_gt_"; break;
		case LEQ: name << "_leq_"; break;
		case GEQ: name << "_geq_"; break;
		default:  name << "_cmp_"; break;
	}

	auto driver1 = getDriver(1);
	if (driver1.node == nullptr) return "";
	if (inputIsComingThroughParentNodeGroup(1)) return "";
	if (driver1.node->getName().empty()) return "";

	name << driver1.node->getName();

	return name.str();
}



void Node_Compare::estimateSignalDelay(SignalDelay &sigDelay)
{
	auto inDelay0 = sigDelay.getDelay(getDriver(0));
	auto inDelay1 = sigDelay.getDelay(getDriver(1));

	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0});

	auto width = getOutputConnectionType(0).width;


	float routing = width * 0.8f;
	float compute;
	
	if (m_op == EQ || m_op == NEQ)
		compute = width * 0.15f; // still a reduce, but maybe slightly faster than through half adders
	else
		compute = width * 0.2f; // same as addition/subtraction

	// very rough for now
	float maxDelay = 0.0f;
	for (auto i : utils::Range(width)) {
		maxDelay = std::max(maxDelay, inDelay0[i]);
		maxDelay = std::max(maxDelay, inDelay1[i]);
	}

	for (auto i : utils::Range(width)) {
		outDelay[i] = maxDelay + routing + compute;
	}
}

void Node_Compare::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	auto inDelay0 = sigDelay.getDelay(getDriver(0));
	auto inDelay1 = sigDelay.getDelay(getDriver(1));

	auto width = getOutputConnectionType(0).width;

	float maxDelay = 0.0f;
	size_t maxIP = 0;
	size_t maxIB = 0;

	for (auto i : utils::Range(width))
		if (inDelay0[i] > maxDelay) {
			maxDelay = inDelay0[i];
			maxIP = 0;
			maxIB = i;
		}

	for (auto i : utils::Range(width))
		if (inDelay1[i] > maxDelay) {
			maxDelay = inDelay1[i];
			maxIP = 1;
			maxIB = i;
		}

	inputPort = maxIP;
	inputBit = maxIB;
}


}
