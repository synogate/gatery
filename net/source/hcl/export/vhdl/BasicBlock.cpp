/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "BasicBlock.h"

#include "Entity.h"
#include "Block.h"
#include "Process.h"
#include "CodeFormatting.h"
#include "AST.h"

#include "GenericMemoryEntity.h"


#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/Clock.h"


namespace hcl::core::vhdl {


void NodeGroupInfo::buildFrom(hlim::NodeGroup *nodeGroup, bool mergeAreasReccursive)
{
    std::vector<hlim::NodeGroup*> nodeGroupStack = { nodeGroup };

    while (!nodeGroupStack.empty()) {
        hlim::NodeGroup *group = nodeGroupStack.back();
        nodeGroupStack.pop_back();

        for (auto node : group->getNodes()) {
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

    for (auto node : m_externalNodes) {
        for (auto i : utils::Range(node->getNumInputPorts())) {
            auto driver = node->getDriver(i);
            if (driver.node != nullptr) {
                if (isProducedExternally(driver))
                    m_inputs.insert(driver);
            }
        }
        for (auto i : utils::Range(node->getNumOutputPorts())) {
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
            if (node->getClocks()[i] != nullptr)
                m_inputClocks.insert(node->getClocks()[i]);
        }
    }
    verifySignalsDisjoint();
}

void BasicBlock::allocateNames()
{
    for (auto &local : m_localSignals)
        m_namespaceScope.allocateName(local, findNearestDesiredName(local), CodeFormatting::SIG_LOCAL_SIGNAL);

    for (auto &proc : m_processes)
        proc->allocateNames();
    for (auto &ent : m_entities)
        ent->allocateNames();
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
    for (auto &ioPin : child->getIoPins()) {
        m_ioPins.insert(ioPin);
    }

    verifySignalsDisjoint();
}


void BasicBlock::collectInstantiations(hlim::NodeGroup *nodeGroup, bool reccursive)
{
    std::vector<hlim::NodeGroup*> nodeGroupStack = { nodeGroup };

    while (!nodeGroupStack.empty()) {
        hlim::NodeGroup *group = nodeGroupStack.back();
        nodeGroupStack.pop_back();

        for (auto node : group->getNodes()) {
            hlim::Node_External *extNode = dynamic_cast<hlim::Node_External *>(node);
            if (extNode != nullptr)
                handleExternalNodeInstantiaton(extNode);
        }

        for (const auto &childGroup : group->getChildren()) {
            if (childGroup->isEmpty(true))
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
    m_externalNodes.push_back(externalNode);
    m_externalNodeInstanceNames.push_back(m_namespaceScope.allocateInstanceName(externalNode->getName()));
    m_ast.getMapping().assignNodeToScope(externalNode, this);

    ConcurrentStatement statement;
    statement.type = ConcurrentStatement::TYPE_EXT_NODE_INSTANTIATION;
    statement.ref.externalNodeIdx = m_externalNodes.size()-1;
    statement.sortIdx = 0; /// @todo

    m_statements.push_back(statement);
}

void BasicBlock::handleSFUInstantiaton(hlim::NodeGroup *sfu)
{
    //Entity *entity;
    if (auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(sfu)) {
        auto &memEntity = m_ast.createSpecialEntity<GenericMemoryEntity>(sfu->getName(), this);
        m_entities.push_back(&memEntity);
        m_entityInstanceNames.push_back(m_namespaceScope.allocateInstanceName(sfu->getInstanceName()));
        memEntity.buildFrom(memGrp);

        //entity = &memEntity;
    } else
        throw hcl::utils::InternalError(__FILE__, __LINE__, "Unhandled SFU node group");

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
            hlim::Node_External *extNode = dynamic_cast<hlim::Node_External *>(node);
            if (extNode != nullptr)
                continue;

            hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
            if (regNode != nullptr) {
                //hlim::NodePort resetValue = regNode->getDriver(hlim::Node_Register::RESET_VALUE);

                RegisterConfig config = {
                    .clock = regNode->getClocks()[0],
                    .hasResetSignal = regNode->getNonSignalDriver(hlim::Node_Register::RESET_VALUE).node != nullptr
                };
                registerNodes[config].push_back(regNode);
                continue;
            }

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
        process.buildFromNodes(normalNodes);

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
                auto *node = m_externalNodes[statement.ref.externalNodeIdx];
                cf.indent(stream, indent);
                stream << m_externalNodeInstanceNames[statement.ref.externalNodeIdx] << " : entity " << node->getName() << " port map (" << std::endl;

                std::vector<std::string> portmapList;

                for (auto i : utils::Range(node->getClocks().size()))
                    if (node->getClocks()[i] != nullptr) {
                        std::stringstream line;
                        line << node->getClockNames()[i] << " => ";
                        line << m_namespaceScope.getName(node->getClocks()[i]);
                        portmapList.push_back(line.str());
                        if (node->getClocks()[i]->getResetType() != hlim::Clock::ResetType::NONE && !node->getResetNames()[i].empty()) {
                            std::stringstream line;
                            line << node->getResetNames()[i] << " => ";
                            line << m_namespaceScope.getName(node->getClocks()[i])<<node->getClocks()[i]->getResetName();
                            portmapList.push_back(line.str());
                        }
                    }

                for (auto i : utils::Range(node->getNumInputPorts()))
                    if (node->getDriver(i).node != nullptr) {
                        std::stringstream line;
                        line << node->getInputName(i) << " => ";
                        line << m_namespaceScope.getName(node->getDriver(i));
                        portmapList.push_back(line.str());
                    }

                for (auto i : utils::Range(node->getNumOutputPorts())) {
                    std::stringstream line;
                    line << node->getOutputName(i) << " => ";
                    line << m_namespaceScope.getName({.node = node, .port = i});
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
