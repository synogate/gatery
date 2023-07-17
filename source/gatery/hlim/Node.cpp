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
#include "Node.h"

#include "NodeGroup.h"
#include "Clock.h"
#include "SignalDelay.h"

#include "../utils/Exceptions.h"
#include "../utils/Range.h"

#include <algorithm>

namespace gtry::hlim {

BaseNode::BaseNode()
{
}

BaseNode::BaseNode(size_t numInputs, size_t numOutputs)
{
	resizeInputs(numInputs);
	resizeOutputs(numOutputs);
}

BaseNode::~BaseNode()
{
	HCL_ASSERT_NOTHROW(m_refCounter == 0);
	moveToGroup(nullptr);
	for (auto i : utils::Range(m_clocks.size()))
		detachClock(i);
}


bool BaseNode::isOrphaned() const
{
	for (auto i : utils::Range(getNumInputPorts()))
		if (getDriver(i).node != nullptr) return false;

	for (auto i : utils::Range(getNumOutputPorts()))
		if (!getDirectlyDriven(i).empty()) return false;

	return true;
}


bool BaseNode::hasSideEffects() const
{
	return false;
}

bool BaseNode::isCombinatorial(size_t port) const
{
	for (auto clk : m_clocks)
		if (clk != nullptr)
			return false;
	return true;
}


void BaseNode::bypassOutputToInput(size_t outputPort, size_t inputPort)
{
	// Usually bypassOutputToInput is used to remove a node.
	// If a comment is associated with this node, pass it on to the driving node.
	auto *driverNode = getDriver(inputPort).node;
	if (driverNode != nullptr) {
		if (!driverNode->m_comment.empty() && !m_comment.empty())
			driverNode->m_comment += '\n';
		driverNode->m_comment += m_comment;
	}
	NodeIO::bypassOutputToInput(outputPort, inputPort);
}

void BaseNode::moveToGroup(NodeGroup *group)
{
	if (group == m_nodeGroup) return;

	if (m_nodeGroup != nullptr) {
		auto it = std::find(m_nodeGroup->m_nodes.begin(), m_nodeGroup->m_nodes.end(), this);
		HCL_ASSERT(it != m_nodeGroup->m_nodes.end());

		*it = m_nodeGroup->m_nodes.back();
		m_nodeGroup->m_nodes.pop_back();
	}

	m_nodeGroup = group;
	if (m_nodeGroup != nullptr)
		m_nodeGroup->m_nodes.push_back(this);
}

Circuit *BaseNode::getCircuit()
{
	if (m_nodeGroup != nullptr)
		return &m_nodeGroup->getCircuit();
	else
		return nullptr;
}

const Circuit *BaseNode::getCircuit() const
{
	if (m_nodeGroup != nullptr)
		return &m_nodeGroup->getCircuit();
	else
		return nullptr;
}

void BaseNode::addClock(Clock *clk)
{
	m_clocks.push_back(nullptr);
	attachClock(clk, m_clocks.size()-1);
}

void BaseNode::attachClock(Clock *clk, size_t clockPort)
{
	if (m_clocks[clockPort] == clk) return;

	detachClock(clockPort);

	m_clocks[clockPort] = clk;
	if (clk != nullptr) {
		clk->m_clockedNodes.emplace(NodePort{.node = this, .port = clockPort});
		if (!clk->m_clockedNodesCache.empty())
			clk->m_clockedNodesCache.emplace_back(NodePort{ .node = this, .port = clockPort });
	}
}

void BaseNode::detachClock(size_t clockPort)
{
	if (m_clocks[clockPort] == nullptr) return;

	auto clock = m_clocks[clockPort];
	clock->m_clockedNodes.erase(NodePort{ .node = this, .port = clockPort });
	clock->m_clockedNodesCache.clear();

	m_clocks[clockPort] = nullptr;
}

OutputClockRelation BaseNode::getOutputClockRelation(size_t output) const
{
	OutputClockRelation res;
	res.dependentInputs.reserve(getNumInputPorts());
	for (auto i : utils::Range(getNumInputPorts()))
		res.dependentInputs.push_back(i);

	if (!m_clocks.empty()) {
		HCL_ASSERT_HINT(m_clocks.size() == 1, "Missing specialized implementation of getOutputClockRelation for node");
		res.dependentClocks.push_back(m_clocks[0]);
	}

	return res;
}

bool BaseNode::checkValidInputClocks(std::span<SignalClockDomain> inputClocks) const
{
	Clock *clock = nullptr;
	size_t numUnknowns = 0;

	if (!m_clocks.empty()) {
		HCL_ASSERT_HINT(m_clocks.size() == 1, "Missing specialized implementation of getOutputClockRelation for node");

		if (m_clocks[0] != nullptr) {
			if (clock == nullptr)
				clock = m_clocks[0]->getClockPinSource();
			else
				if (clock != m_clocks[0]->getClockPinSource()) 
					return false;
		}
	}

	for (auto j : utils::Range(getNumInputPorts())) {
		switch (inputClocks[j].type) {
			case SignalClockDomain::CONSTANT:
			break;
			case SignalClockDomain::UNKNOWN:
				numUnknowns++;
			break;
			case SignalClockDomain::CLOCK:
				if (clock == nullptr)
					clock = inputClocks[j].clk->getClockPinSource();
				else
					if (clock != inputClocks[j].clk->getClockPinSource()) 
						return false;
			break;
		}
	}

	if (numUnknowns > 1 || (numUnknowns > 0 && clock != nullptr))
		return false;

	return true;
}

void BaseNode::copyBaseToClone(BaseNode *copy) const
{
	copy->m_name = m_name;
	copy->m_comment = m_comment;
	copy->m_stackTrace = m_stackTrace;
	copy->m_clocks.resize(m_clocks.size());
	copy->resizeInputs(getNumInputPorts());
	copy->resizeOutputs(getNumOutputPorts());
	for (auto i : utils::Range(getNumOutputPorts())) {
		copy->setOutputConnectionType(i, getOutputConnectionType(i));
		copy->setOutputType(i, getOutputType(i));
	}
}


bool BaseNode::inputIsComingThroughParentNodeGroup(size_t inputPort) const
{
	auto driver = getDriver(inputPort);
	if (driver.node == nullptr) return false;
	return driver.node->getGroup() != m_nodeGroup && !driver.node->getGroup()->isChildOf(m_nodeGroup);
}


std::string BaseNode::attemptInferOutputName(size_t outputPort) const
{
	std::stringstream name;
	bool first = true;
	for (auto i : utils::Range(getNumInputPorts())) {
		auto driver = getDriver(i);
		if (driver.node == nullptr)
			return "";
		if (hlim::outputIsDependency(driver)) continue;
		
		if (inputIsComingThroughParentNodeGroup(i)) return "";

		if (driver.node->getName().empty()) {
			return "";
		} else {
			if (!first) name << '_';
			first = false;
			name << driver.node->getName();
		}
	}
	return name.str();
}


void BaseNode::estimateSignalDelay(SignalDelay &sigDelay)
{
	HCL_ASSERT_HINT(getNumOutputPorts() == 0, "estimateSignalDelay not implemented for node type " + getTypeName());
}

void BaseNode::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t& inputPort, size_t& inputBit)
{
	HCL_ASSERT_HINT(false, "estimateSignalDelayCriticalInput not implemented for node type " + getTypeName());
}

void BaseNode::forwardSignalDelay(SignalDelay &sigDelay, unsigned input, unsigned output)
{
	auto driver = getDriver(input);

	HCL_ASSERT(sigDelay.contains({.node = this, .port = output}));
	auto out = sigDelay.getDelay({.node = this, .port = output});

	if (driver.node == nullptr) {
		for (auto i : utils::Range(out.size()))
			out[i] = 0.0f;
	} else {
		auto in = sigDelay.getDelay(getDriver(input));
		HCL_ASSERT(in.size() == out.size());
		for (auto i : utils::Range(out.size()))
			out[i] = in[i];
	}
}


}
