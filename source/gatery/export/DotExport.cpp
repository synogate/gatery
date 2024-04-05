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

#include "DotExport.h"
#include <gatery/utils/StableContainers.h>
#include "../hlim/Circuit.h"
#include "../hlim/Clock.h"
#include "../hlim/NodeGroup.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Multiplexer.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Logic.h"
#include "../hlim/coreNodes/Node_Arithmetic.h"
#include "../hlim/coreNodes/Node_Compare.h"
#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/supportNodes/Node_Memory.h"
#include "../hlim/supportNodes/Node_MemPort.h"
#include "../hlim/supportNodes/Node_SignalTap.h"

#include "../hlim/Subnet.h"
#include "../hlim/SignalDelay.h"

#include <fstream>
#include <map>
#include <vector>
#include <set>
#include <stdexcept>
#include <cstdlib>

// Adopted from Andy's codebase

namespace gtry {

DotExport::DotExport(std::filesystem::path destination) : m_destination(std::move(destination))
{
}

void DotExport::operator()(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet)
{
	if (m_mergeCombinatoryNodes)
		writeMergedDotFile(circuit, subnet);
	else
		writeDotFile(circuit, subnet, nullptr, nullptr);
}

void DotExport::operator()(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet, const hlim::SignalDelay &signalDelays)
{
	writeDotFile(circuit, subnet, nullptr, &signalDelays);
}

void DotExport::operator()(const hlim::Circuit &circuit, hlim::NodeGroup *nodeGroup)
{
	if (m_mergeCombinatoryNodes && nodeGroup == nullptr)
		writeMergedDotFile(circuit, hlim::ConstSubnet::all(circuit));
	else
		writeDotFile(circuit, hlim::ConstSubnet::all(circuit), nodeGroup, nullptr);
}

void DotExport::writeDotFile(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet, hlim::NodeGroup *nodeGroup, const hlim::SignalDelay *signalDelays)
{
	std::fstream file(m_destination.string().c_str(), std::fstream::out);
	if (!file.is_open())
		throw std::runtime_error("Could not open file!");
	file << "digraph G {" << std::endl;

	utils::StableMap<hlim::BaseNode*, unsigned> node2idx;
//  utils::UnstableMap<hlim::NodeGroup*, unsigned> nodeGroup2idx;


	auto styleNode = [](std::fstream &file, hlim::BaseNode *node) {
		if (dynamic_cast<hlim::Node_Register*>(node))
			file << R"( shape="box" style="filled" fillcolor="#a0a0ff")";
		else if (dynamic_cast<hlim::Node_Constant*>(node))
			file << R"( shape="ellipse" style="filled" fillcolor="#ffa0a0")";
		else if (dynamic_cast<hlim::Node_Multiplexer*>(node))
			file << R"( shape="diamond" style="filled" fillcolor="#b0b0b0")";
		else if (dynamic_cast<hlim::Node_Arithmetic*>(node))
			file << R"( shape="box" style="filled" fillcolor="#a0ffa0")";
		else if (dynamic_cast<hlim::Node_Logic*>(node))
			file << R"( shape="box" style="filled" fillcolor="#ffffa0")";
		else if (dynamic_cast<hlim::Node_Compare*>(node))
			file << R"( shape="box" style="filled" fillcolor="#ffd0a0")";
		else if (dynamic_cast<hlim::Node_Pin*>(node))
			file << " shape=\"house\"";
		else if (dynamic_cast<hlim::Node_SignalTap*>(node))
			file << " shape=\"cds\"";
		else
			if (node->hasRef())
				file << R"( shape="box" style="filled" fillcolor="#eeeeee")";
			else
				file << " shape=\"box\"";

	};

	{
		unsigned idx = 0;
		unsigned graphIdx = 0;

		std::function<void(const hlim::NodeGroup *nodeGroup)> reccurWalkNodeGroup;
		reccurWalkNodeGroup = [&](const hlim::NodeGroup *nodeGroup) {

			file << "subgraph cluster_" << graphIdx << "{" << std::endl;
		//  nodeGroup2idx[nodeGroup] = graphIdx++;
			graphIdx++;

			file << " label=\"" << nodeGroup->getInstanceName() << "\";" << std::endl;
			switch (nodeGroup->getGroupType()) {
				case hlim::NodeGroupType::ENTITY:
					file << " color=blue;" << std::endl;
				break;
				case hlim::NodeGroupType::AREA:
					file << " color=black; style=filled; fillcolor=azure; " << std::endl;
				break;
				case hlim::NodeGroupType::SFU:
					file << " color=black; style=filled; fillcolor=beige;" << std::endl;
				break;
			}

			for (const auto &subGroup : nodeGroup->getChildren())
				reccurWalkNodeGroup(subGroup.get());

			for (auto *node : nodeGroup->getNodes()) {
				if (!subnet.contains(node)) continue;
				
				file << "node_" << idx << "[label=\"";
				if (node->getName().length() < 30)
					file << node->getName();
				else
					file << "[zip]";
				file << " - " << node->getId() << " - " << node->getTypeName();
				if (auto* reg = dynamic_cast<hlim::Node_Register*>(node)) {
					file << '[';
					if (reg->getFlags().contains(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD))
						file << 'F';
					if (reg->getFlags().contains(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD))
						file << 'B';
					file << ']';
				}
				for (auto* clk : node->getClocks())
					if(clk)
						file << ' ' << clk->getName();
				file << "\"";
				file << " id=\"" << node->getId() << "\"";
				if (dynamic_cast<hlim::Node_Signal*>(node))
				{
					file << " tooltip=\"";
					for (std::string trace : node->getStackTrace().formatEntriesFiltered())
						file << trace << "\n";
					file << "\"";
				}
				styleNode(file, node);
				file << "];" << std::endl;
				node2idx[node] = idx;
				idx++;
			}

			file << "}" << std::endl;
		};
		if (nodeGroup == nullptr)
			reccurWalkNodeGroup(circuit.getRootNodeGroup());
		else
			reccurWalkNodeGroup(nodeGroup);

		for (auto &node : circuit.getNodes()) {
			if (node->getGroup() == nullptr) {
				if (!subnet.contains(node.get())) continue;
				file << "node_" << idx << "[label=\"" << node->getName() << " - " << node->getId() << " - " << node->getTypeName();
				if (auto *reg = dynamic_cast<hlim::Node_Register*>(node.get())) {
					file << '[';
					if (reg->getFlags().contains(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD))
						file << 'F';
					if (reg->getFlags().contains(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD))
						file << 'B';
					file << ']';
				}
				file << "\"";
				styleNode(file, node.get());
				file << "];" << std::endl;
				node2idx[node.get()] = idx;
				idx++;
			}
		}
	}

	for (auto &nodeIdx : node2idx) {
		auto &node = nodeIdx.first;
		unsigned nodeId = nodeIdx.second;

		for (auto port : utils::Range(node->getNumInputPorts())) {
			auto producer = node->getDriver(port);
			if (producer.node == nullptr) continue;

			auto producerIt = node2idx.find(producer.node);
			if (producerIt == node2idx.end()) continue;
			
			unsigned producerId = node2idx.find(producer.node)->second;

			auto type = hlim::getOutputConnectionType(producer);

			file << "node_" << producerId << " -> node_" << nodeId << " [";

			switch (producer.node->getOutputType(producer.port)) {
				case hlim::NodeIO::OUTPUT_LATCHED:
					file << " style=\"dashed\"";
				break;
				case hlim::NodeIO::OUTPUT_CONSTANT:
				case hlim::NodeIO::OUTPUT_IMMEDIATE:
				default:
				break;
			}


			std::int64_t creationDistance = (std::int64_t)node->getId() - (std::int64_t)producer.node->getId();
			double weight;
			if (creationDistance > 0)
				weight = 100 / std::log(1+std::abs(creationDistance));
			else {
				//file << " constraint=false";
				weight = 1 / std::log(1+std::abs(creationDistance));
			}

			if (producer.node->getGroup() != node->getGroup())
				weight *= 0.01;

			file << " weight="<<std::round(1+weight * 100); // dot wants integers, so scale everything up

			std::string auxLabel;
			if (signalDelays) {
				auto delay = signalDelays->getDelay(producer);

				float maxDelay = 0.0;
				for (auto f : delay)
					maxDelay = std::max(maxDelay, f);

				auxLabel = std::to_string(maxDelay);

				maxDelay /= 32.0f;
				int red = std::min<int>(255, (int)std::floor(std::max(0.0f, maxDelay - 1.0f) * 255));
				int blue = std::min<int>(255, (int)std::floor(std::max(0.0f, 1.0f-maxDelay) * 255));

				file << (boost::format(" color=\"#%02x00%02x\"") % red % blue);
			}

			file << " label=\"";
			switch (type.type) {
				case hlim::ConnectionType::BOOL:
					file << "Bit"; break;
				case hlim::ConnectionType::BITVEC:
					file << "Vec(" << type.width << ')'; break;
				case hlim::ConnectionType::DEPENDENCY:
					file << "DEPENDENCY"; break;
			}

			if (dynamic_cast<hlim::Node_Register*>(node)) {
				if (port == hlim::Node_Register::Input::RESET_VALUE)
					file << " (reset)";
				else if (port == hlim::Node_Register::Input::ENABLE)
					file << " (en)";
			}

			if (dynamic_cast<hlim::Node_Multiplexer*>(node)) {
				if (port == 0)
					file << " (sel)";
				else
					file << " (" << (port - 1) << ")";
			}

			if (!auxLabel.empty())
				file << " " << auxLabel;
			//file << "\"";
			file << "\" id=\"" << node->getId() << "\"";

			file << "];" << std::endl;
		}
	}

	file << "}" << std::endl;
}


void DotExport::writeMergedDotFile(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet)
{
	struct CombinatoryArea {
		utils::StableSet<const hlim::NodeGroup*> nodeGroups;
		std::vector<const hlim::BaseNode*> nodes;
		std::vector<const hlim::Node_Register*> inputRegisters;
		std::vector<const hlim::Node_Register*> outputRegisters;
	};

	std::vector<CombinatoryArea> areas;
	utils::UnstableMap<const hlim::BaseNode*, size_t> node2idx;
	std::vector<const hlim::Node_Register*> registers;
	std::vector<const hlim::Node_Memory*> memories;
	std::vector<const hlim::Node_Signal*> signals;

	const bool mergeLatched = true;
	const bool hideInternalRegisters = false;
	const bool hideInternalMemories = false;

	{
		utils::UnstableSet<const hlim::BaseNode*> handledNodes;
		std::vector<const hlim::BaseNode*> openList;

		for (auto n : subnet) {
			if (auto *reg = dynamic_cast<const hlim::Node_Register*>(n)) {
				registers.push_back(reg);

				continue;
			}
			if (auto *sig = dynamic_cast<const hlim::Node_Signal*>(n)) {
				signals.push_back(sig);
				continue;
			}
			if (dynamic_cast<const hlim::Node_Constant*>(n)) continue;
			//if (dynamic_cast<const hlim::Node_MemPort*>(n)) continue;

			openList.clear();
			openList.push_back(n);

			CombinatoryArea newArea;

			while (!openList.empty()) {
				auto *node = openList.back();
				openList.pop_back();
				if (handledNodes.contains(node)) continue;
				handledNodes.insert(node);

				if (auto *mem = dynamic_cast<const hlim::Node_Memory*>(node)) {
					memories.push_back(mem);
					continue;
				}

				newArea.nodes.push_back(node);

				//bool followInputs

				for (auto i : utils::Range(node->getNumInputPorts())) {
					auto driver = node->getNonSignalDriver(i);
					if (driver.node ==  nullptr) continue;
					if (dynamic_cast<const hlim::Node_Constant*>(driver.node)) {
						// Don't include to not merge stuff that is actually independent
					} else if (auto *reg = dynamic_cast<const hlim::Node_Register*>(driver.node)) {
						newArea.inputRegisters.push_back(reg);
					} else {
						if (mergeLatched || driver.node->getOutputType(i) != hlim::NodeIO::OUTPUT_LATCHED)
							openList.push_back(driver.node);
					}
				}


				for (auto i : utils::Range(node->getNumOutputPorts())) {
					for (auto nh : const_cast<hlim::BaseNode*>(node)->exploreOutput(i)) {
						if (nh.isSignal()) continue;

						if (dynamic_cast<const hlim::Node_Constant*>(nh.node())) {
							// Don't include to not merge stuff that is actually independent
						} else if (auto *reg = dynamic_cast<const hlim::Node_Register*>(nh.node())) {
							newArea.outputRegisters.push_back(reg);
						} else {
							if (mergeLatched || nh.node()->getOutputType(nh.port()) != hlim::NodeIO::OUTPUT_LATCHED)
								openList.push_back(nh.node());
						}
						nh.backtrack();
					}				
				}				
			}

			if (!newArea.nodes.empty()) {
				areas.push_back(std::move(newArea));
				for (auto n : areas.back().nodes) {
					node2idx[n] = areas.size()-1;
					if (!dynamic_cast<const hlim::Node_Signal*>(n))
						areas.back().nodeGroups.insert(n->getGroup());
				}
			}
		}
	}



	std::set<size_t> connectedNodes;
	for (auto *sig : signals) {
		connectedNodes.clear();

		for (auto i : { 0 }) {
			auto nonSignalDriver = sig->getNonSignalDriver(i);
			if (nonSignalDriver.node ==  nullptr) continue;

			auto it = node2idx.find(nonSignalDriver.node);
			if (it != node2idx.end()) {
				connectedNodes.insert(it->second);
			}
		}
		for (auto nh : const_cast<hlim::Node_Signal*>(sig)->exploreOutput(0)) {
			if (nh.isSignal()) continue;

			auto it = node2idx.find(nh.node());
			if (it != node2idx.end()) {
				connectedNodes.insert(it->second);
			}
			nh.backtrack();
		}

		for (auto c : connectedNodes)
			if (c < areas.size())
				areas[c].nodes.push_back(sig);
	}

	for (auto idx : utils::Range(memories.size())) {
		node2idx[memories[idx]] = areas.size() + idx;
	}

	for (auto i : utils::Range(registers.size())) {
		auto *reg = registers[i];
		node2idx[reg] = areas.size()+memories.size()+i;
	}	

	std::vector<bool> hideNode(node2idx.size(), false);


	struct Connection {
		std::string label;
	};

	std::map<std::pair<size_t, size_t>, Connection> connections;

	for (auto idx : utils::Range(memories.size())) {
		auto *mem = memories[idx];
		connectedNodes.clear();

		for (auto port  : mem->getPorts()) {
			auto it = node2idx.find(port.node);
			if (it != node2idx.end()) {
				connectedNodes.insert(it->second);
				connections[std::pair<size_t, size_t>( it->second, node2idx[mem] )].label += "memory dependency";
				connections[std::pair<size_t, size_t>( node2idx[mem], it->second )].label += "memory dependency";
			}
		}
		if (hideInternalMemories && connectedNodes.size() == 1)
			hideNode[areas.size() + idx] = true;
	}

	for (auto i : utils::Range(registers.size())) {
		auto *reg = registers[i];
		connectedNodes.clear();

		//for (auto i : { hlim::Node_Register::Input::DATA, hlim::Node_Register::Input::ENABLE, hlim::Node_Register::Input::RESET_VALUE }) {
		for (auto i : { hlim::Node_Register::Input::DATA, hlim::Node_Register::Input::ENABLE }) {
			auto driver = reg->getDriver(i);
			auto nonSignalDriver = reg->getNonSignalDriver(i);
			if (nonSignalDriver.node ==  nullptr) continue;

			auto it = node2idx.find(nonSignalDriver.node);
			if (it != node2idx.end()) {
				connectedNodes.insert(it->second);
				std::pair<size_t, size_t> link = { it->second, node2idx[reg] };
				connections[link].label += driver.node->attemptInferOutputName(driver.port);
			}
		}
		for (auto nh : const_cast<hlim::Node_Register*>(reg)->exploreOutput(0)) {
			if (nh.isSignal()) continue;

			auto it = node2idx.find(nh.node());
			if (it != node2idx.end()) {
				connectedNodes.insert(it->second);
				std::pair<size_t, size_t> link = { node2idx[reg], it->second };
				auto &con = connections[link];
				con.label = reg->attemptInferOutputName(0) + "\\n";
			}
			nh.backtrack();
		}

		if (hideInternalRegisters && connectedNodes.size() == 1)
			hideNode[areas.size()+memories.size() + i] = true;
	}




	std::fstream file(m_destination.string().c_str(), std::fstream::out);
	if (!file.is_open())
		throw std::runtime_error("Could not open file!");
	file << "digraph G {" << std::endl;

	for (auto idx : utils::Range(areas.size())) {
		if (hideNode[idx]) continue;
		file << "node_" << idx << "[label=\"";
		file << "Area_" << idx << "\\n";
		for (auto *g : areas[idx].nodeGroups) {
			file << g->getInstanceName() << " : " << g->getName() << "\\n";
		}
		std::map<std::string, size_t> nodeCount;
		for (auto *n : areas[idx].nodes) {
			if (const auto *pin = dynamic_cast<const hlim::Node_Pin*>(n))
				file << "io-pin: " << pin->getName() << "\\n";
			nodeCount[n->getTypeName()]++;
		}
/*		
		for (auto &p : nodeCount)
			file << "node type: " << p.first << " : " << p.second << "\\n";
*/		
		file << "\"";
		file << " shape=\"box\"";
		file << "];" << std::endl;

		{
			std::string filename = (boost::format("area_%i") % idx).str();

			hlim::ConstSubnet areaSubnet;
			for (auto *n : areas[idx].nodes)
				//if (subnet.contains(n))
					areaSubnet.add(n);

			DotExport subexport(m_destination.parent_path()/(filename+".dot"));
			subexport(circuit, areaSubnet);
			subexport.runGraphViz(m_destination.parent_path()/(filename+".svg"));
		}
	}


	for (auto idx : utils::Range(memories.size())) {
		if (hideNode[areas.size() + idx]) continue;
		file << "node_" << areas.size() + idx << "[label=\"";
		file << "Memory " << memories[idx]->getName() << "\\n";
		if (memories[idx]->getGroup() != nullptr)
			file << memories[idx]->getGroup()->getInstanceName() << " : " << memories[idx]->getGroup()->getName() << "\\n";

		file << "\"";
		file << " shape=\"box\" style=\"filled\" fillcolor=\"beige\"";
		file << "];" << std::endl;
	}


	for (auto i : utils::Range(registers.size())) {
		if (hideNode[areas.size()+memories.size() + i]) continue;

		auto *reg = registers[i];

		auto type = reg->getOutputConnectionType(0);
		file << "node_" << areas.size()+memories.size() + i << "[label=\"";
		file << "Register ";
		switch (type.type) {
			case hlim::ConnectionType::BOOL:
				file << "BOOL"; break;
			case hlim::ConnectionType::BITVEC:
				file << "UInt(" << type.width << ')'; break;
			case hlim::ConnectionType::DEPENDENCY:
				file << "DEPENDENCY"; break;
		}
		file << '[';
		if (reg->getFlags().contains(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD))
			file << 'F';
		if (reg->getFlags().contains(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD))
			file << 'B';
		if (reg->getFlags().contains(hlim::Node_Register::Flags::IS_BOUND_TO_MEMORY))
			file << 'M';
		file << ']';

		file << "\"";
		file << " shape=\"box\" style=\"filled\" fillcolor=\"#a0a0ff\"";
		file << "];" << std::endl;
	}	


	for (auto &connection : connections) {
		if (hideNode[connection.first.first] || hideNode[connection.first.second]) continue;

		file << "node_" << connection.first.first << " -> node_" << connection.first.second << " [";

		file << " label=\"";
		file << connection.second.label;
		file << "\"";

		bool bothRegisters = connection.first.first > areas.size() && connection.first.second > areas.size();

		if (bothRegisters)
			file << " weight=1000";
		else
			file << " weight=1";

		file << "];" << std::endl;
	}

	file << "}" << std::endl;
}


void DotExport::runGraphViz(std::filesystem::path destination)
{
	system((std::string("dot -Tsvg ") + m_destination.string() + " -o " + destination.string()).c_str());
}




void visualize(const hlim::Circuit &circuit, const std::string &filename, hlim::NodeGroup *nodeGroup)
{
	DotExport exp(filename+".dot");
	exp(circuit, nodeGroup);
	exp.runGraphViz(filename+".svg");
}

void visualize(const hlim::Circuit &circuit, const std::string &filename, const hlim::ConstSubnet &subnet)
{
	DotExport exp(filename+".dot");
	exp(circuit, subnet);
	exp.runGraphViz(filename+".svg");
}


}
