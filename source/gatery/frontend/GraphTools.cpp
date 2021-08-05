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

#include "GraphTools.h"

#include "DesignScope.h"

#include "../hlim/NodeGroup.h"
#include "../hlim/Circuit.h"
#include "../hlim/coreNodes/Node_Signal.h"

#include <set>

namespace gtry {


BVec hookBVecBefore(hlim::Node_Signal *signal)
{
	HCL_DESIGNCHECK_HINT(signal->getOutputConnectionType(0).interpretation == hlim::ConnectionType::BITVEC, "Attempting to create BVec hook from a signal node that is not a BVec");

	BVec res = SignalReadPort(signal->getDriver(0));
	signal->connectInput(res.getOutPort());
	return res;
}

BVec hookBVecAfter(hlim::Node_Signal *signal)
{
	HCL_DESIGNCHECK_HINT(signal->getOutputConnectionType(0).interpretation == hlim::ConnectionType::BITVEC, "Attempting to create BVec hook from a signal node that is not a BVec");

	BVec res = BitWidth(signal->getOutputConnectionType(0).width);
	while (!signal->getDirectlyDriven(0).empty()) {
		auto np = signal->getDirectlyDriven(0).front();
		np.node->rewireInput(np.port, res.getOutPort());
	}
	res = SignalReadPort(signal);
	return res;
}

Bit hookBitBefore(hlim::Node_Signal *signal)
{
	HCL_DESIGNCHECK_HINT(signal->getOutputConnectionType(0).interpretation == hlim::ConnectionType::BOOL, "Attempting to create Bit hook from a signal node that is not a Bit");

	Bit res = SignalReadPort(signal->getDriver(0));
	signal->connectInput(res.getOutPort());
	return res;
}

Bit hookBitAfter(hlim::Node_Signal *signal)
{
	HCL_DESIGNCHECK_HINT(signal->getOutputConnectionType(0).interpretation == hlim::ConnectionType::BOOL, "Attempting to create Bit hook from a signal node that is not a Bit");

	Bit res;
	while (!signal->getDirectlyDriven(0).empty()) {
		auto np = signal->getDirectlyDriven(0).front();
		np.node->rewireInput(np.port, res.getOutPort());
	}
	res = SignalReadPort(signal);
	return res;
}


NodeGroupIO::NodeGroupIO(hlim::NodeGroup *nodeGroup)
{	
	std::set<hlim::NodePort> inputs;
	std::set<hlim::NodePort> outputs;

	for (auto &n : nodeGroup->getNodes()) {
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (driver.node && driver.node->getGroup() != nodeGroup && !driver.node->getGroup()->isChildOf(nodeGroup))
				inputs.insert({.node=n, .port=i});
		}
		for (auto i : utils::Range(n->getNumOutputPorts())) {
			for (auto driven : n->getDirectlyDriven(i)) {
				if (driven.node && driven.node->getGroup() != nodeGroup && !driven.node->getGroup()->isChildOf(nodeGroup))
					outputs.insert({.node=n, .port=i});
			}
		}
	}
	
	std::set<std::string> usedNames;
	for (auto np : inputs) {
		auto *signal = dynamic_cast<hlim::Node_Signal*>(np.node);
		HCL_ASSERT_HINT(signal, "First node of signal entering node group must be a signal node at this stage!");
		HCL_ASSERT_HINT(!usedNames.contains(signal->getName()), "input-output signal name duplicates!");
		usedNames.insert(signal->getName());

		switch (signal->getOutputConnectionType(0).interpretation) {
			case hlim::ConnectionType::BITVEC:
				inputBVecs[signal->getName()] = hookBVecBefore(signal);
			break;
			case hlim::ConnectionType::BOOL:
				inputBits[signal->getName()] = hookBitBefore(signal);
			break;
		}
	}
	usedNames.clear();
	for (auto np : outputs) {
		auto *signal = dynamic_cast<hlim::Node_Signal*>(np.node);
		HCL_ASSERT_HINT(signal, "Last node of signal leaving node group must be a signal node at this stage!");
		HCL_ASSERT_HINT(!usedNames.contains(signal->getName()), "input-output signal name duplicates!");
		usedNames.insert(signal->getName());

		switch (signal->getOutputConnectionType(0).interpretation) {
			case hlim::ConnectionType::BITVEC:
				outputBVecs[signal->getName()] = hookBVecAfter(signal);
			break;
			case hlim::ConnectionType::BOOL:
				outputBits[signal->getName()] = hookBitAfter(signal);
			break;
		}
	}
}


NodeGroupSurgeryHelper::NodeGroupSurgeryHelper(hlim::NodeGroup *nodeGroup)
{
	for (auto &node : nodeGroup->getNodes()) 
		if (auto *signal = dynamic_cast<hlim::Node_Signal*>(node))
			if (!signal->getName().empty() && !signal->nameWasInferred())
				m_namedSignalNodes[signal->getName()].push_back(signal);
}

bool NodeGroupSurgeryHelper::containsSignal(std::string_view name)
{
	return m_namedSignalNodes.contains(name);
}

BVec NodeGroupSurgeryHelper::hookBVecBefore(std::string_view name)
{
	auto it = m_namedSignalNodes.find(name);
	HCL_DESIGNCHECK_HINT(it != m_namedSignalNodes.end(), "Named signal was not found in node group!");
	HCL_DESIGNCHECK_HINT(it->second.size() == 1, "Named signal is ambigous (exists multiple times) in node group!");
	return gtry::hookBVecBefore(it->second.front());
}

BVec NodeGroupSurgeryHelper::hookBVecAfter(std::string_view name)
{
	auto it = m_namedSignalNodes.find(name);
	HCL_DESIGNCHECK_HINT(it != m_namedSignalNodes.end(), "Named signal was not found in node group!");
	HCL_DESIGNCHECK_HINT(it->second.size() == 1, "Named signal is ambigous (exists multiple times) in node group!");
	return gtry::hookBVecAfter(it->second.front());
}

Bit NodeGroupSurgeryHelper::hookBitBefore(std::string_view name)
{
	auto it = m_namedSignalNodes.find(name);
	HCL_DESIGNCHECK_HINT(it != m_namedSignalNodes.end(), "Named signal was not found in node group!");
	HCL_DESIGNCHECK_HINT(it->second.size() == 1, "Named signal is ambigous (exists multiple times) in node group!");
	return gtry::hookBitBefore(it->second.front());
}

Bit NodeGroupSurgeryHelper::hookBitAfter(std::string_view name)
{
	auto it = m_namedSignalNodes.find(name);
	HCL_DESIGNCHECK_HINT(it != m_namedSignalNodes.end(), "Named signal was not found in node group!");
	HCL_DESIGNCHECK_HINT(it->second.size() == 1, "Named signal is ambigous (exists multiple times) in node group!");
	return gtry::hookBitAfter(it->second.front());
}

Bit NodeGroupSurgeryHelper::getBit(std::string_view name)
{
	auto it = m_namedSignalNodes.find(name);
	HCL_DESIGNCHECK_HINT(it != m_namedSignalNodes.end(), "Named signal was not found in node group!");
	HCL_DESIGNCHECK_HINT(it->second.size() == 1, "Named signal is ambigous (exists multiple times) in node group!");

	auto *signal = it->second.front();
	HCL_DESIGNCHECK_HINT(signal->getOutputConnectionType(0).interpretation == hlim::ConnectionType::BOOL, "Attempting to create Bit hook from a signal node that is not a Bit");

	return SignalReadPort(signal);
}

BVec NodeGroupSurgeryHelper::getBVec(std::string_view name)
{
	auto it = m_namedSignalNodes.find(name);
	HCL_DESIGNCHECK_HINT(it != m_namedSignalNodes.end(), "Named signal was not found in node group!");
	HCL_DESIGNCHECK_HINT(it->second.size() == 1, "Named signal is ambigous (exists multiple times) in node group!");

	auto *signal = it->second.front();
	HCL_DESIGNCHECK_HINT(signal->getOutputConnectionType(0).interpretation == hlim::ConnectionType::BITVEC, "Attempting to create Bit hook from a signal node that is not a Bit");

	return SignalReadPort(signal);
}

const std::vector<hlim::Node_Signal*> &NodeGroupSurgeryHelper::getAllSignals(std::string_view name)
{
	auto it = m_namedSignalNodes.find(name);
	if (it == m_namedSignalNodes.end()) 
		return m_empty;
	return it->second;
}


sim::DefaultBitVectorState evaluateStatically(hlim::NodePort output)
{
	return hlim::evaluateStatically(DesignScope::get()->getCircuit(), output);
}


sim::DefaultBitVectorState evaluateStatically(const Bit &bit)
{
	return evaluateStatically(bit.getReadPort());
}

sim::DefaultBitVectorState evaluateStatically(const BVec &bvec)
{
	return evaluateStatically(bvec.getReadPort());
}



hlim::Node_Pin *findInputPin(ElementarySignal &sig)
{
	return hlim::findInputPin(sig.getReadPort());
}

hlim::Node_Pin *findOutputPin(ElementarySignal &sig)
{
	return hlim::findOutputPin(sig.getReadPort());
}

}

