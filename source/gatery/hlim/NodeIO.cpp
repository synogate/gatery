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
#include "NodeIO.h"

#include "Node.h"

#include "coreNodes/Node_Signal.h"

#include "../utils/Range.h"

#include <algorithm>

namespace gtry::hlim {

NodeIO::~NodeIO()
{
	resizeInputs(0);
	resizeOutputs(0);
}

hlim::ConnectionType NodeIO::getDriverConnType(size_t inputPort) const
{
	NodePort driver = getDriver(inputPort);
	return driver.node->getOutputConnectionType(driver.port);
}

NodePort NodeIO::getDriver(size_t inputPort) const
{
	HCL_ASSERT(inputPort < m_inputPorts.size());
	return m_inputPorts[inputPort];
}

NodePort NodeIO::getNonSignalDriver(size_t inputPort) const
{
	NodePort np = m_inputPorts[inputPort];
	size_t loopCounter = 0;
	const NodeIO* loopCheckSignal = this;

	while (np.node != nullptr && dynamic_cast<Node_Signal*>(np.node) != nullptr)
	{
		if (np.node == loopCheckSignal)
			return NodePort{};
		if (++loopCounter % 200 == 0) // TODO: use node color for loop detection
			loopCheckSignal = np.node;

		np = np.node->m_inputPorts[0];
	}
	return np;
}

const std::vector<NodePort> &NodeIO::getDirectlyDriven(size_t outputPort) const
{
	return m_outputPorts[outputPort].connections;
}

/*
ExplorationList NodeIO::getSignalsDriven(size_t outputPort) const
{
}

ExplorationList NodeIO::getNonSignalDriven(size_t outputPort) const
{
}
*/
void NodeIO::setOutputConnectionType(size_t outputPort, const ConnectionType &connectionType) 
{ 
	if (m_outputPorts[outputPort].connectionType != connectionType) {
		HCL_ASSERT_HINT(m_outputPorts[outputPort].connections.empty(), "The connection type of the output can not change once a node has connected to it!");		
		m_outputPorts[outputPort].connectionType = connectionType; 
	}
}

void NodeIO::setOutputType(size_t outputPort, OutputType outputType)
{
	m_outputPorts[outputPort].outputType = outputType;
}


void NodeIO::connectInput(size_t inputPort, const NodePort &output)
{
	auto &inPort = m_inputPorts[inputPort];
	if (inPort.node == output.node && inPort.port == output.port)
		return;
	
	if (inPort.node != nullptr)
		disconnectInput(inputPort);
	
	inPort = output;
	if (inPort.node != nullptr) {
		auto &outPort = inPort.node->m_outputPorts[inPort.port];
		outPort.connections.push_back({.node = static_cast<BaseNode*>(this), .port = inputPort});
	}
}

void NodeIO::disconnectInput(size_t inputPort)
{
	auto &inPort = m_inputPorts[inputPort];
	if (inPort.node != nullptr) {
		auto &outPort = inPort.node->m_outputPorts[inPort.port];
		
		auto it = std::find(
						outPort.connections.begin(),
						outPort.connections.end(),
						NodePort{.node = static_cast<BaseNode*>(this), .port = inputPort});
		
		HCL_ASSERT(it != outPort.connections.end());
		
		std::swap(*it, outPort.connections.back());
		outPort.connections.pop_back();
		
		inPort.node = nullptr;
		inPort.port = INV_PORT;
	}
}

void NodeIO::resizeInputs(size_t num)
{
	if (num < m_inputPorts.size())
		for (auto i : utils::Range(num, m_inputPorts.size()))
			disconnectInput(i);
	m_inputPorts.resize(num);
}

void NodeIO::resizeOutputs(size_t num)
{
	if (num < m_outputPorts.size())
		for (auto i : utils::Range(num, m_outputPorts.size()))
			while (!m_outputPorts[i].connections.empty()) {
				auto &con = m_outputPorts[i].connections.front();
				con.node->disconnectInput(con.port);
			}
			
	m_outputPorts.resize(num);
}

void NodeIO::bypassOutputToInput(size_t outputPort, size_t inputPort)
{
	NodePort newSource = getDriver(inputPort);
	
	while (!getDirectlyDriven(outputPort).empty()) {
		auto p = getDirectlyDriven(outputPort).front();
		p.node->connectInput(p.port, newSource);
	}
}
		
}
