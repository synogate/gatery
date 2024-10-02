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
#include "Node_Arithmetic.h"

#include <gatery/simulation/BitVectorState.h>

#include "../../utils/BitManipulation.h"
#include "../../utils/Exceptions.h"
#include "../../utils/Preprocessor.h"

#include "../SignalDelay.h"

#include <boost/multiprecision/cpp_int.hpp>

namespace gtry::hlim {

Node_Arithmetic::Node_Arithmetic(Op op, size_t operands) : Node(operands, 1), m_op(op)
{

}

void Node_Arithmetic::connectInput(size_t operand, const NodePort &port)
{
	NodeIO::connectInput(operand, port);
	updateConnectionType();
}

void Node_Arithmetic::updateConnectionType()
{
	std::optional<ConnectionType> desiredConnectionType;
	for (size_t i = 0; i < getNumInputPorts(); i++) {
		auto driver = getDriver(i);

		if (driver.node != nullptr) {
			if (!desiredConnectionType)
				desiredConnectionType = hlim::getOutputConnectionType(driver);
			else {
				desiredConnectionType->width = std::max(desiredConnectionType->width, getOutputWidth(driver));
				HCL_ASSERT_HINT(desiredConnectionType->type == driver.node->getOutputConnectionType(driver.port).type, "Mixing different interpretations not yet implemented!");
			}
		}
	}

	if (!desiredConnectionType)
		desiredConnectionType = getOutputConnectionType(0);

	setOutputConnectionType(0, *desiredConnectionType);
}


void Node_Arithmetic::disconnectInput(size_t operand)
{
	NodeIO::disconnectInput(operand);
}

void Node_Arithmetic::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{

	for (size_t i = 0; i < getNumInputPorts(); i++) {
		auto driver = getDriver(i);
		if (inputOffsets[i] == ~0ull || driver.node == nullptr) {
			state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
			return;
		}

		const auto &type = hlim::getOutputConnectionType(driver);
		if (!allDefined(state, inputOffsets[i], type.width)) {
			state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
			return;
		}
	}


	state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, true);

	if (getOutputConnectionType(0).width <= 64) {
		std::uint64_t result = 0;

		for (size_t i = 0; i < getNumInputPorts(); i++) {
			auto driver = getDriver(i);
			const auto &type = hlim::getOutputConnectionType(driver);
		
			std::uint64_t value = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[i], type.width);

			if (i == 0)
				result = value;
			else {
				switch (getOutputConnectionType(0).type) {
					case ConnectionType::BOOL:
						HCL_ASSERT_HINT(false, "Can't do arithmetic on booleans!");
					break;
					case ConnectionType::BITVEC:
						switch (m_op) {
							case ADD:
								result += value;
							break;
							case SUB:
								result -= value;
							break;
							case MUL:
								result *= value;
							break;
							case DIV:
								if (value != 0)
									result /= value;
								else
									state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
							break;
							case REM:
								if (value != 0)
									result = result % value;
								else
									state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
							break;
							default:
								HCL_ASSERT_HINT(false, "Unhandled case!");
						}
					break;
					default:
						HCL_ASSERT_HINT(false, "Unhandled case!");
				}
			}
		}

		state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], getOutputConnectionType(0).width, result);
	} else {
		sim::BigInt value, result;

		for (size_t i = 0; i < getNumInputPorts(); i++) {
			auto driver = getDriver(i);
			const auto &type = hlim::getOutputConnectionType(driver);

			value = sim::extractBigInt(state, inputOffsets[i], type.width);

			if (i == 0)
				result = value;
			else {
				switch (getOutputConnectionType(0).type) {
					case ConnectionType::BOOL:
						HCL_ASSERT_HINT(false, "Can't do arithmetic on booleans!");
					break;
					case ConnectionType::BITVEC:
						switch (m_op) {
							case ADD:
								result += value;
							break;
							case SUB:
								result -= value;
							break;
							case MUL:
								result *= value;
							break;
							case DIV:
								if (value != 0)
									result /= value;
								else
									state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
							break;
							case REM:
								if (value != 0)
									result = result % value;
								else
									state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
							break;
							default:
								HCL_ASSERT_HINT(false, "Unhandled case!");
						}
					break;
					default:
						HCL_ASSERT_HINT(false, "Unhandled case!");
				}
			}
		}
		sim::insertBigInt(state, outputOffsets[0], getOutputConnectionType(0).width, result);
	}
}


std::string Node_Arithmetic::getTypeName() const
{
	switch (m_op) {
		case ADD: return "add";
		case SUB: return "sub";
		case MUL: return "mul";
		case DIV: return "div";
		case REM: return "remainder";
		default: return "Arithmetic";
	}
}

void Node_Arithmetic::assertValidity() const
{

}

std::string Node_Arithmetic::getInputName(size_t idx) const
{
	std::string name("a");
	name[0] += (char)idx;
	return name;
}

std::string Node_Arithmetic::getOutputName(size_t idx) const
{
	return "out";
}

std::unique_ptr<BaseNode> Node_Arithmetic::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_Arithmetic(m_op, getNumInputPorts()));
	copyBaseToClone(res.get());
	return res;
}

std::string Node_Arithmetic::attemptInferOutputName(size_t outputPort) const
{
	std::stringstream name;

	auto driver0 = getDriver(0);
	if (driver0.node == nullptr) return "";
	if (inputIsComingThroughParentNodeGroup(0)) return "";
	if (driver0.node->getName().empty()) return "";

	name << driver0.node->getName();

	switch (m_op) {
		case ADD: name << "_plus_"; break;
		case SUB: name << "_minus_"; break;
		case MUL: name << "_times_"; break;
		case DIV: name << "_over_"; break;
		case REM: name << "_rem_"; break;
		default: name << "_arith_op_"; break;
	}

	auto driver1 = getDriver(1);
	if (driver1.node == nullptr) return "";
	if (inputIsComingThroughParentNodeGroup(1)) return "";
	if (driver1.node->getName().empty()) return "";

	name << driver1.node->getName();

	return name.str();
}


void Node_Arithmetic::estimateSignalDelay(SignalDelay &sigDelay)
{
	auto inDelay0 = sigDelay.getDelay(getDriver(0));
	auto inDelay1 = sigDelay.getDelay(getDriver(1));

	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});

	auto width = getOutputConnectionType(0).width;


	switch (m_op) {
		case ADD:
		case SUB: {
			float routing = width * 0.8f;
			float compute = width * 0.2f;

			// very rough for now
			float maxDelay = 0.0f;
			for (auto i : utils::Range(width)) {
				maxDelay = std::max(maxDelay, inDelay0[i]);
				maxDelay = std::max(maxDelay, inDelay1[i]);
			}
			for (auto i : utils::Range(width)) {
				outDelay[i] = maxDelay + routing + compute;
			}
		} break;
		case MUL:
		case DIV:
		case REM:
		default:
			HCL_ASSERT_HINT(false, "Unhandled arithmetic operation for timing analysis!");
		break;
	}
}

void Node_Arithmetic::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
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
