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
#include "../../hlim/coreNodes/Node_MultiDriver.h"
#include "../../hlim/coreNodes/Node_Pin.h"
#include "../../hlim/supportNodes/Node_ExportOverride.h"
#include "../../hlim/GraphTools.h"
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

	for (auto &extNode : m_externalNodes) {
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

	for (auto &assignment : m_assignments) {
		if (isProducedExternally(assignment.source))
			m_inputs.insert(assignment.source);
		else
			m_localSignals.insert(assignment.source);

		if (assignment.enable.node != nullptr) {
			if (isProducedExternally(assignment.enable))
				m_inputs.insert(assignment.enable);
			else
				m_localSignals.insert(assignment.enable);
		}
		if (isConsumedExternally(assignment.destination))
			m_outputs.insert(assignment.destination);
		else
			m_localSignals.insert(assignment.destination);
	}

	// In case no assignments outside of port maps happen with this
	for (auto m : m_multiDriverNodes) {
		hlim::NodePort np{.node = m, .port = 0ull};
		if (isConsumedExternally(np))
			m_outputs.insert(np);
		else
			m_localSignals.insert(np);
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

			if (auto *extNode = dynamic_cast<hlim::Node_External *>(node))
				handleExternalNodeInstantiation(extNode);
			else
			if (auto *multiNode = dynamic_cast<hlim::Node_MultiDriver *>(node))
				handleMultiDriverNodeInstantiation(multiNode);
				/*
			else
			if (auto *pin = dynamic_cast<hlim::Node_Pin *>(node))
				handlePinInstantiation(pin);
				*/
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
					handleSFUInstantiation(childGroup.get());
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

void BasicBlock::handleExternalNodeInstantiation(hlim::Node_External *externalNode)
{
	m_externalNodes.push_back({
		.node = externalNode,
		.instanceName = m_namespaceScope.allocateInstanceName(externalNode->getName()+"_inst"),
	});
	m_ast.getMapping().assignNodeToScope(externalNode, this);

	auto desiredFilenames = externalNode->getSupportFiles();
	if (!desiredFilenames.empty()) {
		std::string prefix = getInstanceName() + '_' + m_externalNodes.back().instanceName + '_';
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


void BasicBlock::handleMultiDriverNodeInstantiation(hlim::Node_MultiDriver *multi)
{
	m_multiDriverNodes.push_back(multi);
	for (auto i : utils::Range(multi->getNumInputPorts())) {
		const auto driver = hlim::findDriver(multi, {.inputPortIdx = i, .skipExportOverrideNodes = hlim::Node_ExportOverride::EXP_INPUT});
		if (driver.node == nullptr) continue;

		// Check if this is a direct inout link to an external node.
		// In this case, only the "output" (which poses as the inout) needs to be bound to the multi driver
		// which happens in the port map of the instantiation of the external node.
		// No Assignment of the input is necessary.
		if (auto *extNode = dynamic_cast<hlim::Node_External*>(driver.node))
			if (extNode->getOutputPorts()[driver.port].bidirPartner)
				if (extNode->getNonSignalDriver(*extNode->getOutputPorts()[driver.port].bidirPartner).node == multi)
					continue;

		m_assignments.push_back(AssignmentInstance{
			.enable = {},
			.source = multi->getDriver(i),
			.destination = {.node = multi, .port = 0ull},
		});

		ConcurrentStatement statement;
		statement.type = ConcurrentStatement::TYPE_ASSIGNMENT;
		statement.ref.assignmentIdx = m_assignments.size()-1;
		statement.sortIdx = 0; /// @todo

		m_statements.push_back(statement);
	}

	m_ast.getMapping().assignNodeToScope(multi, this);
}

void BasicBlock::handlePinInstantiation(hlim::Node_Pin *pin)
{
	if (pin->isOutputPin()) {
		const auto &driver = pin->getDriver(0);
		if (driver.node != nullptr) {
			hlim::NodePort cond = pin->getDriver(1);

			m_assignments.push_back(AssignmentInstance{
				.enable = cond,
				.source = driver,
				.destination = {.node = pin, .port = 0ull},
			});

			ConcurrentStatement statement;
			statement.type = ConcurrentStatement::TYPE_ASSIGNMENT;
			statement.ref.assignmentIdx = m_assignments.size()-1;
			statement.sortIdx = 0; /// @todo

			m_statements.push_back(statement);
		}
	}

	m_ast.getMapping().assignNodeToScope(pin, this);
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

void BasicBlock::handleSFUInstantiation(hlim::NodeGroup *sfu)
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

			if (auto *extNode = dynamic_cast<hlim::Node_External *>(node))
				continue;

			if (auto *multiDriverNode = dynamic_cast<hlim::Node_MultiDriver *>(node))
				continue;

		//	if (auto *pin = dynamic_cast<hlim::Node_Pin *>(node))
			//	continue;

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

			std::set<hlim::NodePort> visited;
			for (auto i : utils::Range(node->getNumOutputPorts())) {
				for (auto nh : node->exploreOutput(i)) {
					if (visited.contains(nh.nodePort())) {
						nh.backtrack();
						continue;
					}
					visited.insert(nh.nodePort());

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
				if (!node->requiresComponentDeclaration()) {
					if (!node->getLibraryName().empty())
						stream << node->getLibraryName() << '.';
					if (!node->getPackageName().empty())
						stream << node->getPackageName() << '.';
				}
				stream << node->getName() << std::endl;
				
				if (!node->getGenericParameters().empty()) {
					cf.indent(stream, indent);
					stream << " generic map (" << std::endl;

					unsigned i = 0;
					for (const auto &p : node->getGenericParameters()) {
						cf.indent(stream, indent+1);
						stream << p.first << " => ";
						
						const auto &param = p.second;
						if (param.isDecimal())
						 	stream << param.decimal();
						else if (param.isReal())
						 	stream << param.real(); 
						else if (param.isBoolean())
						 	stream << (param.boolean()?"true":"false");
						else if (param.isString())
						 	stream << '"' << param.string() << '"';
						else if (param.isBit())
						 	stream << '\'' << param.bit() << '\'';
						else if (param.isBitVector())
						 	stream << '"' << param.bitVector() << '"';
						else	
							HCL_ASSERT_HINT(false, "Unhandled generic parameter type!");

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

				for (auto i : utils::Range(node->getClocks().size())) {
					if (!node->getClockNames()[i].empty()) {
						std::stringstream line;
						line << node->getClockNames()[i] << " => ";
						if (node->getClocks()[i] != nullptr)
							line << m_namespaceScope.getClock(node->getClocks()[i]->getClockPinSource()).name;
						else
							line << "'X'";
						portmapList.push_back(line.str());
					}
					if (!node->getResetNames()[i].empty()) {
						std::stringstream line;
						line << node->getResetNames()[i] << " => ";
						if (node->getClocks()[i]->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE)
							line << m_namespaceScope.getReset(node->getClocks()[i]->getResetPinSource()).name;
						else
							line << "'X'";
						portmapList.push_back(line.str());
					}
				}

				bool nodeHasExplicitComponentDeclaration = node->requiresComponentDeclaration();

				for (auto i : utils::Range(node->getNumInputPorts())) {
					bool connected = node->getDriver(i).node != nullptr;
					if (nodeHasExplicitComponentDeclaration || connected) {
						std::stringstream line;
						line << node->getInputName(i) << " => ";

						if (node->getDriver(i).node != nullptr) {

							const auto &decl = m_namespaceScope.get(node->getDriver(i));
							if (decl.dataType == VHDLDataType::UNSIGNED)
								line << "STD_LOGIC_VECTOR(";
							line << decl.name;
							if (decl.dataType == VHDLDataType::UNSIGNED)
								line << ')';
						} else {
							if (node->getInputPorts()[i].isVector)
								line << "(others => 'X')";
							else
								line << "'X'";
						}
						portmapList.push_back(line.str());
					}
				}

				for (auto i : utils::Range(node->getNumOutputPorts())) {
					const auto &type = node->getOutputPorts()[i];
					if (type.bidirPartner) continue;

					bool connected = !node->getDirectlyDriven(i).empty();
					if (nodeHasExplicitComponentDeclaration || connected) {
						std::stringstream line;
						if (connected) {
							const auto &decl = m_namespaceScope.get({.node = node, .port = i});
							if (decl.dataType == VHDLDataType::UNSIGNED)
								line << "UNSIGNED(" << node->getOutputName(i) << ") => " << decl.name;
							else
								line << node->getOutputName(i) << " => " << decl.name;
						} else {
							line << node->getOutputName(i) << " => open";
						}
						portmapList.push_back(line.str());
					}
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
			case ConcurrentStatement::TYPE_ASSIGNMENT: {
				const auto &assignment = m_assignments[statement.ref.assignmentIdx];

				cf.indent(stream, indent);

				// No signal nodes between multi driver and io pins, so need explicit handling of those

				const auto &dstDecl = m_namespaceScope.get(assignment.destination);
				stream << dstDecl.name << " <= ";

				if (auto *ioPinNode = dynamic_cast<hlim::Node_Pin *>(assignment.source.node)) {
					const auto &srcDecl = m_namespaceScope.get(ioPinNode);
					stream << srcDecl.name;
				} else {
					const auto &srcDecl = m_namespaceScope.get(assignment.source);
					stream << srcDecl.name;
				}

				if (assignment.enable.node == nullptr)
					stream << ';' << std::endl;
				else {
					const auto &condDecl = m_namespaceScope.get(assignment.enable);
					stream << " when (" << condDecl.name << " = '1') else ";

					if (hlim::getOutputConnectionType(assignment.enable).interpretation == hlim::ConnectionType::BOOL)
						stream << "'Z';" << std::endl;
					else
						stream << "(others => 'Z');" << std::endl;
				}
				/// @todo: potentially lost comments
			} break;
		}
	}
}


void BasicBlock::declareLocalComponents(std::ostream &stream, size_t indentation)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();
	
	std::set<std::string> alreadyDeclaredComponents;

	for (auto &n : m_externalNodes) {
		auto node = n.node;
		if (!node->requiresComponentDeclaration()) continue;

		if (alreadyDeclaredComponents.contains(node->getName())) continue;
		alreadyDeclaredComponents.insert(node->getName());


		cf.indent(stream, indentation);
		stream << "COMPONENT " << node->getName() << '\n';
		if (!node->getGenericParameters().empty()) {
			cf.indent(stream, indentation+1);
			stream << "GENERIC (\n";

			std::vector<std::string> lines;

			for (const auto &p : node->getGenericParameters()) {
				std::stringstream line;
				line << p.first << " : ";

				cf.formatGenericParameterType(line, p.second);

				lines.push_back(line.str());
			}

			for (auto i : utils::Range(lines.size())) {
				cf.indent(stream, indentation+2);
				stream << lines[i];
				if (i+1 < lines.size())
					stream << ";";
				stream << std::endl;
			}

			cf.indent(stream, indentation+1);
			stream << ");\n";
		}

		if (node->getNumInputPorts() != 0 || node->getNumOutputPorts() != 0 || !node->getClocks().empty()) {
			cf.indent(stream, indentation+1);
			stream << "PORT (\n";

			std::vector<std::string> portmapList;

			for (auto i : utils::Range(node->getClocks().size())) {
				if (!node->getClockNames()[i].empty()) {
					std::stringstream line;
					line << node->getClockNames()[i] << " : STD_LOGIC";
					portmapList.push_back(line.str());
				}
				if (!node->getResetNames()[i].empty()) {
					std::stringstream line;
					line << node->getResetNames()[i] << " : STD_LOGIC";
					portmapList.push_back(line.str());
				}
			}

			HCL_DESIGNCHECK_HINT(node->getNumInputPorts() == node->getInputPorts().size(), 
				"External nodes that require component declarations must have their ports declared via declInputBit[Vector]!");

			for (auto i : utils::Range(node->getNumInputPorts())) {
				std::stringstream line;

				const auto &type = node->getInputPorts()[i];

				if (type.bidirPartner)
					line << node->getInputName(i) << " : INOUT ";
				else
					line << node->getInputName(i) << " : IN ";

				if (type.isVector) {
					cf.formatBitVectorFlavor(line, std::get<hlim::Node_External::BitVectorFlavor>(type.flavor));
					line << '(';
					if (type.componentWidth.empty())
						line << ((int)type.instanceWidth-1)<< " downto 0)";
					else
						line << type.componentWidth << "-1 downto 0)";
				} else {
					cf.formatBitFlavor(line, std::get<hlim::Node_External::BitFlavor>(type.flavor));
				}

				portmapList.push_back(line.str());
			}

			HCL_DESIGNCHECK_HINT(node->getNumOutputPorts() == node->getOutputPorts().size(), 
				"External nodes that require component declarations must have their ports declared via declOutputBit[Vector]!");

			for (auto i : utils::Range(node->getNumOutputPorts())) {

				const auto &type = node->getOutputPorts()[i];
				if (type.bidirPartner) continue;

				std::stringstream line;

				line << node->getOutputName(i) << " : OUT ";

				if (type.isVector) {
					cf.formatBitVectorFlavor(line, std::get<hlim::Node_External::BitVectorFlavor>(type.flavor));
					line << '(';
					if (type.componentWidth.empty())
						line << ((int)type.instanceWidth-1)<< " downto 0)";
					else
						line << type.componentWidth << "-1 downto 0)";
				} else {
					cf.formatBitFlavor(line, std::get<hlim::Node_External::BitFlavor>(type.flavor));
				}

				portmapList.push_back(line.str());
			}

			for (auto i : utils::Range(portmapList.size())) {
				cf.indent(stream, indentation+2);
				stream << portmapList[i];
				if (i+1 < portmapList.size())
					stream << ";";
				stream << std::endl;
			}

			cf.indent(stream, indentation+1);
			stream << ");\n";
		}


		cf.indent(stream, indentation);
		stream << "END COMPONENT;\n";
	}
}


}
