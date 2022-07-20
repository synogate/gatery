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
#include "BasicBlock.h"

#include "Entity.h"
#include "Block.h"
#include "Process.h"
#include "CodeFormatting.h"
#include "AST.h"

#include "GenericMemoryEntity.h"
#include "../../hlim/postprocessing/MemoryDetector.h"


#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/Clock.h"


namespace gtry::vhdl {




BasicBlock::BasicBlock(AST &ast, BasicBlock *parent, NamespaceScope *parentNamespace) : BaseGrouping(ast, parent, parentNamespace)
{

}

BasicBlock::~BasicBlock()
{

}


void BasicBlock::extractSignals()
{
	for (auto &proc : m_processes) {
		proc->extractSignals();
		routeChildIOUpwards(proc.get());
	}
	for (auto &ent : m_entities) {
		ent->extractSignals();
		routeChildIOUpwards(ent);
	}

	for (auto extNode : m_externalNodes) {
		auto node = extNode.node;
		for (auto i : utils::Range(node->getNumInputPorts())) {
			auto driver = node->getDriver(i);
			if (driver.node != nullptr) {
				if (isProducedExternally(driver))
					m_inputs.insert(driver);
			}
		}
		for (auto i : utils::Range(node->getNumOutputPorts())) {
			if (node->getDirectlyDriven(i).empty()) continue;

			hlim::NodePort driver = {
				.node = node,
				.port = i
			};

			if (isConsumedExternally(driver))
				m_outputs.insert(driver);
			else
				m_localSignals.insert(driver);
		}
		for (auto i : utils::Range(node->getClocks().size())) {
			if (node->getClocks()[i] != nullptr) {
				if (!node->getClockNames()[i].empty())
					m_inputClocks.insert(node->getClocks()[i]->getClockPinSource());
				if (!node->getResetNames()[i].empty())
					m_inputResets.insert(node->getClocks()[i]->getResetPinSource());
			}
		}
	}
	verifySignalsDisjoint();
}

void BasicBlock::allocateNames()
{
	for (auto &constant : m_constants)
		m_namespaceScope.allocateName(constant, findNearestDesiredName(constant), chooseDataTypeFromOutput(constant), CodeFormatting::SIG_CONSTANT);

	for (auto &local : m_localSignals) 
		m_namespaceScope.allocateName(local, findNearestDesiredName(local), chooseDataTypeFromOutput(local), CodeFormatting::SIG_LOCAL_SIGNAL);

	for (auto &proc : m_processes)
		proc->allocateNames();
	for (auto &ent : m_entities)
		ent->allocateNames();
}

bool BasicBlock::findLocalDeclaration(hlim::NodePort driver, std::vector<BaseGrouping*> &reversePath)
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

	return false;
}

void BasicBlock::routeChildIOUpwards(BaseGrouping *child)
{
	verifySignalsDisjoint();

	for (auto &input : child->getInputs()) {
		if (isProducedExternally(input))
			m_inputs.insert(input);
	}
	for (auto &output : child->getOutputs()) {
		if (isConsumedExternally(output))
			m_outputs.insert(output);
		else
			m_localSignals.insert(output);
	}
	for (auto &clock : child->getClocks()) {
		m_inputClocks.insert(clock);
	}
	for (auto &clock : child->getResets()) {
		m_inputResets.insert(clock);
	}
	for (auto &ioPin : child->getIoPins()) {
		m_ioPins.insert(ioPin);
	}

	verifySignalsDisjoint();
}

void BasicBlock::addNeededLibraries(std::set<std::string> &libs) const
{
	for (auto n : m_externalNodes)
		if (!n.node->getLibraryName().empty())
			libs.insert(n.node->getLibraryName());
}


void BasicBlock::collectInstantiations(hlim::NodeGroup *nodeGroup, bool reccursive)
{
	std::vector<hlim::NodeGroup*> nodeGroupStack = { nodeGroup };

	while (!nodeGroupStack.empty()) {
		hlim::NodeGroup *group = nodeGroupStack.back();
		nodeGroupStack.pop_back();

		for (auto node : group->getNodes()) {
			if (!m_ast.isPartOfExport(node)) continue;

			hlim::Node_External *extNode = dynamic_cast<hlim::Node_External *>(node);
			if (extNode != nullptr)
				handleExternalNodeInstantiaton(extNode);
		}

		for (const auto &childGroup : group->getChildren()) {
			if (m_ast.isEmpty(childGroup.get(), true))
				continue;
			switch (childGroup->getGroupType()) {
				case hlim::NodeGroup::GroupType::ENTITY:
					handleEntityInstantiation(childGroup.get());
				break;
				case hlim::NodeGroup::GroupType::AREA:
					if (reccursive)
						nodeGroupStack.push_back(childGroup.get());
				break;
				case hlim::NodeGroup::GroupType::SFU:
					handleSFUInstantiaton(childGroup.get());
				break;
			}
		}
	}
}

void BasicBlock::handleEntityInstantiation(hlim::NodeGroup *nodeGroup)
{
	auto &entity = m_ast.createEntity(nodeGroup->getName(), this);
	m_entities.push_back(&entity);
	m_entityInstanceNames.push_back(m_namespaceScope.allocateInstanceName(nodeGroup->getInstanceName()));
	entity.buildFrom(nodeGroup);

	ConcurrentStatement statement;
	statement.type = ConcurrentStatement::TYPE_ENTITY_INSTANTIATION;
	statement.ref.entityIdx = m_entities.size()-1;
	statement.sortIdx = m_entities.size()-1;

	m_statements.push_back(statement);
}

void BasicBlock::handleExternalNodeInstantiaton(hlim::Node_External *externalNode)
{
	m_externalNodes.push_back({
		.node = externalNode,
		.instanceName = m_namespaceScope.allocateInstanceName(externalNode->getName()),
	});
	m_ast.getMapping().assignNodeToScope(externalNode, this);

	auto desiredFilenames = externalNode->getSupportFiles();
	if (!desiredFilenames.empty()) {
		std::string prefix = getInstanceName() + '_';
		auto *p = m_parent;
		while (p != nullptr) {
			prefix = p->getInstanceName() + '_' + prefix;
			p = p->getParent();
		}

		for (auto i : utils::Range(desiredFilenames.size())) {
			desiredFilenames[i] = m_namespaceScope.allocateSupportFileName(prefix + desiredFilenames[i]);
		}

		m_externalNodes.back().supportFilenames = std::move(desiredFilenames);
	}

	ConcurrentStatement statement;
	statement.type = ConcurrentStatement::TYPE_EXT_NODE_INSTANTIATION;
	statement.ref.externalNodeIdx = m_externalNodes.size()-1;
	statement.sortIdx = 0; /// @todo

	m_statements.push_back(statement);
}

void BasicBlock::writeSupportFiles(const std::filesystem::path &destination) const
{
	for (const auto &extNode : m_externalNodes) {
		for (auto i : utils::Range(extNode.supportFilenames.size())) {
			auto path = destination / extNode.supportFilenames[i];
			std::fstream stream(path.string().c_str(), std::fstream::out | std::fstream::binary);
			extNode.node->setupSupportFile(i, extNode.supportFilenames[i], stream);
		}
	}
}

void BasicBlock::handleSFUInstantiaton(hlim::NodeGroup *sfu)
{
	//Entity *entity;
	if (dynamic_cast<hlim::MemoryGroup*>(sfu->getMetaInfo())) {
		auto &memEntity = m_ast.createSpecialEntity<GenericMemoryEntity>(sfu->getName(), this);
		m_entities.push_back(&memEntity);
		m_entityInstanceNames.push_back(m_namespaceScope.allocateInstanceName(sfu->getInstanceName()));
		memEntity.buildFrom(sfu);

		//entity = &memEntity;
	} else
		throw gtry::utils::InternalError(__FILE__, __LINE__, "Unhandled SFU node group");

	ConcurrentStatement statement;
	statement.type = ConcurrentStatement::TYPE_ENTITY_INSTANTIATION;
	statement.ref.entityIdx = m_entities.size()-1;
	statement.sortIdx = m_entities.size()-1;

	m_statements.push_back(statement);
}


void BasicBlock::processifyNodes(const std::string &desiredProcessName, hlim::NodeGroup *nodeGroup, bool reccursive)
{
	std::vector<hlim::BaseNode*> normalNodes;
	std::map<RegisterConfig, std::vector<hlim::BaseNode*>> registerNodes;


	std::vector<hlim::NodeGroup*> nodeGroupStack = { nodeGroup };

	while (!nodeGroupStack.empty()) {
		hlim::NodeGroup *group = nodeGroupStack.back();
		nodeGroupStack.pop_back();


		for (auto node : group->getNodes()) {
			if (!m_ast.isPartOfExport(node)) continue;

			hlim::Node_External *extNode = dynamic_cast<hlim::Node_External *>(node);
			if (extNode != nullptr)
				continue;

			hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
			if (regNode != nullptr) {
				//hlim::NodePort resetValue = regNode->getDriver(hlim::Node_Register::RESET_VALUE);

				RegisterConfig config = {
					.clock = regNode->getClocks()[0]->getClockPinSource(),
					.triggerEvent = regNode->getClocks()[0]->getTriggerEvent(),
					.resetType = regNode->getClocks()[0]->getRegAttribs().resetType,
				};

				if (regNode->getNonSignalDriver(hlim::Node_Register::RESET_VALUE).node != nullptr && regNode->getClocks()[0]->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE) {
					config.reset = regNode->getClocks()[0]->getResetPinSource();
					config.resetHighActive = regNode->getClocks()[0]->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH;
				}

				registerNodes[config].push_back(regNode);

				auto reset = regNode->getNonSignalDriver(hlim::Node_Register::RESET_VALUE);
				if (reset.node != nullptr && regNode->getClocks()[0]->getRegAttribs().initializeRegs) {
					auto *constReset = dynamic_cast<hlim::Node_Constant*>(reset.node);
					HCL_DESIGNCHECK_HINT(constReset, "Resets of registers must be constants uppon export!");

					m_localSignalDefaultValues[{.node = regNode, .port = 0ull}] = constReset;
				}

				continue;
			}

			bool nodeFeedingIntoAnythingElse = false;
			bool nodeFeedingIntoReset = false;

			for (auto i : utils::Range(node->getNumOutputPorts())) {
				for (auto nh : node->exploreOutput(i)) {
					if (nh.isNodeType<hlim::Node_Register>() && nh.port() == hlim::Node_Register::RESET_VALUE) {
						nodeFeedingIntoReset = true;
						nh.backtrack();						
					} else if (!nh.isSignal()) {
						nodeFeedingIntoAnythingElse = true;
						break;
					}
				}
			}

			bool onlyUsedForReset = nodeFeedingIntoReset && !nodeFeedingIntoAnythingElse;

			if (onlyUsedForReset) continue;
			
			
			
			
			normalNodes.push_back(node);
		}


		if (reccursive)
			for (const auto &childGroup : group->getChildren())
				if (childGroup->getGroupType() == hlim::NodeGroup::GroupType::AREA)
					nodeGroupStack.push_back(childGroup.get());
	}

	if (!normalNodes.empty()) {
		m_processes.push_back(std::make_unique<CombinatoryProcess>(this, desiredProcessName));
		Process &process = *m_processes.back();
		process.buildFromNodes(std::move(normalNodes));

		ConcurrentStatement statement;
		statement.type = ConcurrentStatement::TYPE_PROCESS;
		statement.ref.process = &process;
		statement.sortIdx = 0; /// @todo

		m_statements.push_back(statement);
	}

	for (auto &regProc : registerNodes) {
		m_processes.push_back(std::make_unique<RegisterProcess>(this, desiredProcessName, regProc.first));
		Process &process = *m_processes.back();
		process.buildFromNodes(regProc.second);

		ConcurrentStatement statement;
		statement.type = ConcurrentStatement::TYPE_PROCESS;
		statement.ref.process = &process;
		statement.sortIdx = 0; /// @todo

		m_statements.push_back(statement);
	}
}



void BasicBlock::writeStatementsVHDL(std::ostream &stream, unsigned indent)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	for (auto &statement : m_statements) {
		switch (statement.type) {
			case ConcurrentStatement::TYPE_ENTITY_INSTANTIATION: {
				auto subEntity = m_entities[statement.ref.entityIdx];
				subEntity->writeInstantiationVHDL(stream, indent, m_entityInstanceNames[statement.ref.entityIdx]);
			} break;
			case ConcurrentStatement::TYPE_EXT_NODE_INSTANTIATION: {
				auto *node = m_externalNodes[statement.ref.externalNodeIdx].node;
				cf.indent(stream, indent);
				stream << m_externalNodes[statement.ref.externalNodeIdx].instanceName << " : ";
				if (node->isEntity())
					stream << " entity ";
				stream << node->getLibraryName();
				if (!node->getPackageName().empty())
					stream << '.' << node->getPackageName();
				stream << '.' << node->getName() << std::endl;
				
				if (!node->getGenericParameters().empty()) {
					cf.indent(stream, indent);
					stream << " generic map (" << std::endl;

					unsigned i = 0;
					for (const auto &p : node->getGenericParameters()) {
						cf.indent(stream, indent+1);
						stream << p.first << " => " << p.second;
						if (i+1 < node->getGenericParameters().size())
							stream << ',';
						stream << std::endl;
						i++;
					}

					cf.indent(stream, indent);
					stream << ")" << std::endl;
				}
				
				cf.indent(stream, indent);
				stream << " port map (" << std::endl;

				std::vector<std::string> portmapList;

				for (auto i : utils::Range(node->getClocks().size()))
					if (node->getClocks()[i] != nullptr) {
						if (!node->getClockNames()[i].empty()) {
							std::stringstream line;
							line << node->getClockNames()[i] << " => ";
							line << m_namespaceScope.getClock(node->getClocks()[i]->getClockPinSource()).name;
							portmapList.push_back(line.str());
						}
						if (node->getClocks()[i]->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE && !node->getResetNames()[i].empty()) {
							std::stringstream line;
							line << node->getResetNames()[i] << " => ";
							line << m_namespaceScope.getReset(node->getClocks()[i]->getResetPinSource()).name;
							portmapList.push_back(line.str());
						}
					}


				for (auto i : utils::Range(node->getNumInputPorts()))
					if (node->getDriver(i).node != nullptr) {
						std::stringstream line;
						line << node->getInputName(i) << " => ";

						const auto &decl = m_namespaceScope.get(node->getDriver(i));

						if (decl.dataType == VHDLDataType::UNSIGNED)
							line << "STD_LOGIC_VECTOR(";
						line << decl.name;
						if (decl.dataType == VHDLDataType::UNSIGNED)
							line << ')';
						portmapList.push_back(line.str());
					}

				for (auto i : utils::Range(node->getNumOutputPorts())) {
					if (node->getDirectlyDriven(i).empty()) continue;

					const auto &decl = m_namespaceScope.get({.node = node, .port = i});

					std::stringstream line;
						if (decl.dataType == VHDLDataType::UNSIGNED)
						line << "UNSIGNED(" << node->getOutputName(i) << ") => " << decl.name;
					else
						line << node->getOutputName(i) << " => " << decl.name;
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
			} break;
			case ConcurrentStatement::TYPE_BLOCK:
				HCL_ASSERT(indent == 1);
				statement.ref.block->writeVHDL(stream);
			break;
			case ConcurrentStatement::TYPE_PROCESS:
				statement.ref.process->writeVHDL(stream, indent);
			break;
		}
	}
}

}
