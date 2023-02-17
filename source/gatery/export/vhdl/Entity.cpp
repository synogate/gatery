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
#include "Entity.h"

#include "Block.h"

#include "AST.h"
#include "Package.h"
#include "Process.h"

#include "../../hlim/Clock.h"
#include "../../hlim/coreNodes/Node_Pin.h"


#include <memory>

namespace gtry::vhdl {


struct NodeGroupInfo
{
	std::vector<hlim::BaseNode*> nodes;
	std::vector<hlim::Node_External*> externalNodes;
	std::vector<hlim::NodeGroup*> subEntities;
	std::vector<hlim::NodeGroup*> subAreas;
	std::vector<hlim::NodeGroup*> SFUs;

	void buildFrom(AST &ast, hlim::NodeGroup *nodeGroup, bool mergeAreasReccursive);
};


void NodeGroupInfo::buildFrom(AST &ast, hlim::NodeGroup *nodeGroup, bool mergeAreasReccursive)
{
	std::vector<hlim::NodeGroup*> nodeGroupStack = { nodeGroup };

	while (!nodeGroupStack.empty()) {
		hlim::NodeGroup *group = nodeGroupStack.back();
		nodeGroupStack.pop_back();

		for (auto node : group->getNodes()) {
			if (!ast.isPartOfExport(node)) continue;

			hlim::Node_External *extNode = dynamic_cast<hlim::Node_External *>(node);
			if (extNode != nullptr) {
				externalNodes.push_back(extNode);
			} else
				nodes.push_back(node);
		}

		for (const auto &childGroup : group->getChildren()) {
			switch (childGroup->getGroupType()) {
				case hlim::NodeGroup::GroupType::ENTITY:
					subEntities.push_back(childGroup.get());
				break;
				case hlim::NodeGroup::GroupType::AREA:
					if (mergeAreasReccursive)
						nodeGroupStack.push_back(childGroup.get());
					else
						subAreas.push_back(childGroup.get());
				break;
				case hlim::NodeGroup::GroupType::SFU:
					SFUs.push_back(childGroup.get());
				break;
			}
		}
	}
}




Entity::Entity(AST &ast, const std::string &desiredName, BasicBlock *parent) : BasicBlock(ast, parent, &ast.getNamespaceScope())
{
	m_name = m_ast.getNamespaceScope().allocateEntityName(desiredName);
}

Entity::~Entity()
{

}

void Entity::buildFrom(hlim::NodeGroup *nodeGroup)
{
	HCL_ASSERT(nodeGroup->getGroupType() == hlim::NodeGroup::GroupType::ENTITY);

	m_comment = nodeGroup->getComment();

	NodeGroupInfo grpInfo;
	grpInfo.buildFrom(m_ast, nodeGroup, false);

	collectInstantiations(nodeGroup, false);

	processifyNodes("default", nodeGroup, false);

	for (auto &subArea : grpInfo.subAreas) {
		NodeGroupInfo areaInfo;
		areaInfo.buildFrom(m_ast, subArea, false);

		// If there is nothing but logic inside it's a process, otherwise a block
		if (areaInfo.externalNodes.empty() &&
			areaInfo.subEntities.empty() &&
			areaInfo.subAreas.empty()) {

			processifyNodes(subArea->getName(), subArea, true);
		} else {
			// create block
			m_blocks.push_back(std::make_unique<Block>(this, subArea->getName()));
			auto &block = m_blocks.back();
			block->buildFrom(subArea);

			ConcurrentStatement statement;
			statement.type = ConcurrentStatement::TYPE_BLOCK;
			statement.ref.block = block.get();
			statement.sortIdx = 0; /// @todo

			m_statements.push_back(statement);
		}
	}

	std::sort(m_statements.begin(), m_statements.end());
}

void Entity::extractSignals()
{
	BasicBlock::extractSignals();
	for (auto &block : m_blocks) {
		block->extractSignals();
		routeChildIOUpwards(block.get());
	}
}

void Entity::allocateNames()
{
	for (auto &constant : m_constants)
		m_namespaceScope.allocateName(constant, findNearestDesiredName(constant), chooseDataTypeFromOutput(constant), CodeFormatting::SIG_CONSTANT);

	for (auto &input : m_inputs)
		m_namespaceScope.allocateName(input, findNearestDesiredName(input), chooseDataTypeFromOutput(input), CodeFormatting::SIG_ENTITY_INPUT);

	for (auto &output : m_outputs)
		m_namespaceScope.allocateName(output, findNearestDesiredName(output), chooseDataTypeFromOutput(output), CodeFormatting::SIG_ENTITY_OUTPUT);

	for (auto &clock : m_inputClocks)
		m_namespaceScope.allocateName(clock, clock->getName());

	for (auto &clock : m_inputResets)
		m_namespaceScope.allocateResetName(clock, clock->getResetName());

	for (auto &ioPin : m_ioPins) {
		VHDLDataType dataType;
		if (ioPin->getConnectionType().isBool())
			dataType = VHDLDataType::STD_LOGIC;
		else
			dataType = VHDLDataType::STD_LOGIC_VECTOR;

		std::string desiredName = ioPin->getName();
		if (desiredName.empty()) desiredName = "io";
		m_namespaceScope.allocateName(ioPin, desiredName, dataType);
	}

	BasicBlock::allocateNames();
	for (auto &block : m_blocks)
		block->allocateNames();
}

std::map<std::string, std::set<std::string>> Entity::collectNeededLibraries()
{
	std::map<std::string, std::set<std::string>> libs;
	addNeededLibraries(libs);
	for (auto &block : m_blocks)
		block->addNeededLibraries(libs);

	return libs;
}


void Entity::writeLibrariesVHDL(std::ostream &stream)
{
	stream << "LIBRARY ieee;\n"
		   << "USE ieee.std_logic_1164.ALL;\n"
		   << "USE ieee.numeric_std.ALL;\n\n";

	auto additionalLibs = collectNeededLibraries();
	for (auto &lib : additionalLibs) {
		stream << "LIBRARY " << lib.first << ";\n";
		for (const auto &useDecl : lib.second)
			stream << "USE " << useDecl << ";\n";
		stream << '\n';
	}

	// Import everything for now
	for (const auto &package : m_ast.getPackages())
		package->writeImportStatement(stream);
}

std::vector<std::string> Entity::getPortsVHDL()
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	std::vector<std::pair<size_t, std::string>> unsortedPortList;

	size_t clockOffset = 0;

	bool isRoot = m_parent == nullptr;

	for (const auto &clk : m_inputClocks) {
		if (clk->isSelfDriven(false, true) || !isRoot) {
			std::stringstream line;
			line << m_namespaceScope.getClock(clk).name << " : IN STD_LOGIC";
			unsortedPortList.push_back({clockOffset++, line.str()});
		}
	}

	for (const auto &clk : m_inputResets) {
		if (clk->isSelfDriven(false, false) || !isRoot) {
			std::stringstream line;
			line << m_namespaceScope.getReset(clk).name << " : IN STD_LOGIC";
			unsortedPortList.push_back({clockOffset++, line.str()});
		}
	}

	for (auto &ioPin : m_ioPins) {
		const auto &decl = m_namespaceScope.get(ioPin);

		std::stringstream line;
		line << decl.name << " : ";
		if (ioPin->isInputPin() && ioPin->isOutputPin()) {
			line << "INOUT "; // this will need more thought at other places to work
			cf.formatConnectionType(line, decl);
		} else if (ioPin->isInputPin()) {
			line << "IN ";
			cf.formatConnectionType(line, decl);
		} else if (ioPin->isOutputPin()) {
			line << "OUT ";
			cf.formatConnectionType(line, decl);
		} else
			continue;
		unsortedPortList.push_back({clockOffset + ioPin->getId(), line.str()});
	}

	for (const auto &signal : m_inputs) {
		const auto &decl = m_namespaceScope.get(signal);

		std::stringstream line;
		line << decl.name << " : IN ";
		cf.formatConnectionType(line, decl);
		unsortedPortList.push_back({clockOffset + signal.node->getId(), line.str()});
	}
	for (const auto &signal : m_outputs) {
		const auto &decl = m_namespaceScope.get(signal);

		std::stringstream line;
		line << decl.name << " : OUT ";
		cf.formatConnectionType(line, decl);
		unsortedPortList.push_back({clockOffset + signal.node->getId(), line.str()});
	}

	std::sort(unsortedPortList.begin(), unsortedPortList.end());

	std::vector<std::string> sortedPortList;
	sortedPortList.reserve(unsortedPortList.size());
	for (auto &p : unsortedPortList) sortedPortList.push_back(p.second);
	return sortedPortList;
}

void Entity::writeLocalSignalsVHDL(std::ostream &stream)
{
	declareLocalSignals(stream, false, 0);
}



void Entity::writeVHDL(std::ostream &stream)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	stream << cf.getFileHeader();

	writeLibrariesVHDL(stream);

	cf.formatEntityComment(stream, m_name, m_comment);

	stream << "ENTITY " << m_name << " IS " << std::endl;
	writePortDeclaration(stream, 1);
	stream << "END " << m_name << ";" << std::endl << std::endl;

	stream << "ARCHITECTURE impl OF " << m_name << " IS " << std::endl;

	declareLocalComponents(stream, 1);

	writeLocalSignalsVHDL(stream);

	stream << "BEGIN" << std::endl;

	writeStatementsVHDL(stream, 1);

	stream << "END impl;" << std::endl;
}

void Entity::writePortDeclaration(std::ostream &stream, size_t indentation)
{
	std::vector<std::string> portList = getPortsVHDL();
	if (portList.empty()) 
		return; // Empty "PORT( );" is not allowed in vhdl

	CodeFormatting &cf = m_ast.getCodeFormatting();

	cf.indent(stream, 1); stream << "PORT(" << std::endl;

	for (auto i : utils::Range(portList.size())) {
		cf.indent(stream, 2);
		stream << portList[i];
		if (i+1 < portList.size())
			stream << ';';
		stream << '\n';
	}

	cf.indent(stream, 1); stream << ");" << std::endl;
}

void Entity::writeInstantiationVHDL(std::ostream &stream, unsigned indent, const std::string &instanceName)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	cf.indent(stream, indent);
	stream << instanceName << " : entity work." << getName() << "(impl) port map (" << std::endl;

	std::vector<std::string> portmapList;

	bool isRoot = m_parent == nullptr;

	for (auto &s : m_inputClocks) {
		if (s->isSelfDriven(false, true) || !isRoot) {
			std::stringstream line;
			line << m_namespaceScope.getClock(s).name << " => ";
			line << m_parent->getNamespaceScope().getClock(s).name;
			portmapList.push_back(line.str());
		}
	}
	for (auto &s : m_inputResets) {
		if (s->isSelfDriven(false, false) || !isRoot) {
			std::stringstream line;
			line << m_namespaceScope.getReset(s).name << " => ";
			line << m_parent->getNamespaceScope().getReset(s).name;
			portmapList.push_back(line.str());
		}
	}	
	for (auto &s : m_ioPins) {
		const auto &decl = m_namespaceScope.get(s);
		const auto &parentDecl = m_parent->getNamespaceScope().get(s);

		std::stringstream line;

		if (decl.dataType != parentDecl.dataType) {
			if (s->isInputPin()) {
				line << decl.name << " => ";
				cf.formatDataType(line, decl.dataType);
				line << '(' << parentDecl.name << ')';
			} else {
				cf.formatDataType(line, decl.dataType);
				line << '(' << decl.name << ") => ";
				line << parentDecl.name;
			}
		} else {
			line << decl.name << " => " << parentDecl.name;
		}
		portmapList.push_back(line.str());
	}
	for (auto &s : m_inputs) {
		const auto &decl = m_namespaceScope.get(s);
		const auto &parentDecl = m_parent->getNamespaceScope().get(s);

		std::stringstream line;
		if (decl.dataType != parentDecl.dataType) {
			line << decl.name << " => ";
			cf.formatDataType(line, decl.dataType);
			line << '(' << parentDecl.name << ')';
		} else {
			line << decl.name << " => " << parentDecl.name;
		}
		portmapList.push_back(line.str());
	}
	for (auto &s : m_outputs) {
		const auto &decl = m_namespaceScope.get(s);
		const auto &parentDecl = m_parent->getNamespaceScope().get(s);

		std::stringstream line;
		if (decl.dataType != parentDecl.dataType) {
			cf.formatDataType(line, decl.dataType);
			line << '(' << decl.name << ") => ";
			line << parentDecl.name;
		} else {
			line << decl.name << " => " << parentDecl.name;
		}
		portmapList.push_back(line.str());
	}

	for (auto i : utils::Range(portmapList.size())) {
		cf.indent(stream, indent+1);
		stream << portmapList[i];
		if (i+1 < portmapList.size())
			stream << ",";
		stream << std::endl;
	}


	cf.indent(stream, indent);
	stream << ");" << std::endl;
}


Entity *Entity::getParentEntity()
{
	auto *parent = getParent();
	while (parent != nullptr) {
		if (auto *entity = dynamic_cast<Entity*>(parent))
			return entity;
		else
			parent = parent->getParent();
	}
	return nullptr;
}

bool Entity::findLocalDeclaration(hlim::NodePort driver, std::vector<BaseGrouping*> &reversePath)
{
	if (BaseGrouping::findLocalDeclaration(driver, reversePath))
		return true;
	
	for (auto &p : m_processes) {
		if (p->findLocalDeclaration(driver, reversePath)) {
			reversePath.push_back(this);
			return true;
		}
	}

	for (auto &e : m_entities) {
		if (e->findLocalDeclaration(driver, reversePath)) {
			reversePath.push_back(this);
			return true;
		}
	}

	for (auto &b : m_blocks) {
		if (b->findLocalDeclaration(driver, reversePath)) {
			reversePath.push_back(this);
			return true;
		}
	}

	return false;
}

std::string Entity::getInstanceName()
{
	if (m_parent == nullptr) return "";

	/// @todo: This is ugly
	// We don't track instance names, so loop over all entities of parent until we find ourselves and check what instance name is being used.
	auto *pbb = (const BasicBlock*) m_parent;
	for (auto i : utils::Range(pbb->getSubEntities().size()))
		if (pbb->getSubEntities()[i] == this)
			return pbb->getSubEntityInstanceNames()[i];

	HCL_ASSERT_HINT(false, "Did not find entity instantiation in parent's list of entities!");
}

void Entity::writeSupportFiles(const std::filesystem::path &destination) const
{
	BasicBlock::writeSupportFiles(destination);
	for (auto &b : m_blocks)
		b->writeSupportFiles(destination);
}

}
