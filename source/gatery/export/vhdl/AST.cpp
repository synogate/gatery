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
#include "AST.h"

#include "Entity.h"
#include "CodeFormatting.h"
#include "HelperPackage.h"
#include "Block.h"
#include "InterfacePackage.h"

#include "../../hlim/Circuit.h"
#include "../../hlim/Clock.h"
#include "../../hlim/NodeGroup.h"

#include <gatery/utils/FileSystem.h>

#include <fstream>
#include <functional>

namespace gtry::vhdl {


void Hlim2AstMapping::assignNodeToScope(hlim::BaseNode *node, BaseGrouping *block)
{
	m_node2Block[node] = block;
}

BaseGrouping *Hlim2AstMapping::getScope(hlim::BaseNode *node) const
{
	auto it = m_node2Block.find(node);
	if (it == m_node2Block.end())
		return nullptr;
	return it->second;
}


AST::AST(CodeFormatting *codeFormatting, SynthesisTool *synthesisTool) : m_codeFormatting(codeFormatting), m_synthesisTool(synthesisTool), m_namespaceScope(*this, nullptr)
{
	m_packages.push_back(std::make_unique<HelperPackage>(*this));
}

AST::~AST()
{
}

void AST::generateInterfacePackage(InterfacePackageContent &content)
{
	m_packages.push_back(std::make_unique<InterfacePackage>(*this, content));
}

void AST::convert(hlim::Circuit &circuit)
{
	m_exportArea.addAllForExport(circuit).addDrivenNamedSignals(circuit);

	auto rootNode = circuit.getRootNodeGroup();
	auto &entity = createEntity(rootNode->getName(), nullptr);
	entity.buildFrom(rootNode);
	entity.extractSignals();
	entity.allocateNames();

	for (auto &clk : circuit.getClocks())
		m_namespaceScope.allocateName(clk.get(), clk->getName());
}

Entity &AST::createEntity(const std::string &desiredName, BasicBlock *parent)
{
	m_entities.push_back(std::make_unique<Entity>(*this, desiredName, parent));
	return *m_entities.back();
}

std::filesystem::path AST::getFilename(const std::string &name)
{
	return name + m_codeFormatting->getFilenameExtension();
}

void AST::distributeToFiles(OutputMode outputMode, std::filesystem::path singleFileName, const std::map<std::string, std::string> &customVhdlFiles)
{
	m_sourceFiles.clear();

	if (outputMode == OutputMode::SINGLE_FILE || (outputMode == OutputMode::AUTO && !singleFileName.empty())) {
		m_sourceFiles.resize(1);

		m_sourceFiles.back().filename = singleFileName;
		m_sourceFiles.back().entities.reserve(m_entities.size());
		for (auto *e : getDependencySortedEntities())
			m_sourceFiles.back().entities.push_back(e);

		m_sourceFiles.back().filename = singleFileName;
		m_sourceFiles.back().packages.reserve(m_packages.size());
		for (auto &e : m_packages)
			m_sourceFiles.back().packages.push_back(e.get());

		for (const auto &pair : customVhdlFiles)
			m_sourceFiles.back().customVhdlFiles.push_back(pair.second);

	} else if (outputMode == OutputMode::FILE_PER_PARTITION) {

		for (auto &p : m_packages) {
			m_sourceFiles.push_back({.filename = getFilename(p->getName())});
			m_sourceFiles.back().packages.push_back(p.get());
		}


		std::map<const hlim::NodeGroup*, Entity*> entityMapping;
		for (auto &e : m_entities)
			entityMapping[e->getNodeGroup()] = e.get();


		std::map<Entity*, std::vector<Entity*>> entitiesByPartition;
		for (auto *e : getDependencySortedEntities()) {
			HCL_ASSERT(e->getNodeGroup() != nullptr);
			const hlim::NodeGroup *partition = e->getNodeGroup()->getPartition();
			Entity *entity;
			if (partition != nullptr)
				entity = entityMapping[partition];
			else
				entity = getRootEntity();
			entitiesByPartition[entity].push_back(e);
		}

		m_sourceFiles.reserve(entitiesByPartition.size());
		for (auto &pair : entitiesByPartition) {
			m_sourceFiles.push_back({.filename = getFilename(pair.first->getName())});
			m_sourceFiles.back().entities = pair.second;
		}

		for (const auto &pair : customVhdlFiles) {
			m_sourceFiles.push_back({.filename = getFilename(pair.first)});
			m_sourceFiles.back().customVhdlFiles.push_back(pair.second);
		}

	} else {
		m_sourceFiles.reserve(m_entities.size() + m_packages.size() + customVhdlFiles.size());

		for (auto &p : m_packages) {
			m_sourceFiles.push_back({.filename = getFilename(p->getName())});
			m_sourceFiles.back().packages.push_back(p.get());
		}

		for (const auto &pair : customVhdlFiles) {
			m_sourceFiles.push_back({.filename = getFilename(pair.first)});
			m_sourceFiles.back().customVhdlFiles.push_back(pair.second);
		}

		for (auto &e : m_entities) {
			m_sourceFiles.push_back({.filename = getFilename(e->getName())});
			m_sourceFiles.back().entities.push_back(e.get());
		}
	}
}

void AST::writeVHDL(utils::FileSystem &fileSystem, OutputMode outputMode, std::filesystem::path singleFileName, const std::map<std::string, std::string> &customVhdlFiles)
{
	distributeToFiles(outputMode, singleFileName, customVhdlFiles);

	for (auto& entity : m_entities)
		entity->writeSupportFiles(fileSystem);


	for (const auto &f : m_sourceFiles) {
		auto file = fileSystem.writeFile(f.filename);

		for (auto& package : f.packages)
			package->writeVHDL(file->stream());

		for (const auto &elem : f.customVhdlFiles)
			file->stream() << elem << std::endl;

		for (auto* entity : f.entities)
			entity->writeVHDL(file->stream());
	}
}

std::vector<Entity*> AST::getDependencySortedEntities()
{
	std::vector<Entity*> reverseList;

	std::function<void(Entity*)> reccurEntity;
	reccurEntity = [&](Entity *entity) {
		reverseList.push_back(entity);
		for (auto *subEnt : entity->getSubEntities())
			reccurEntity(subEnt);
		for (auto &block : entity->getBlocks())
			for (auto *subEnt : block->getSubEntities())
				reccurEntity(subEnt);
	};

	reccurEntity(getRootEntity());


	return {reverseList.rbegin(), reverseList.rend()};
}

bool AST::findLocalDeclaration(hlim::NodePort driver, std::vector<BaseGrouping*> &reversePath)
{
	if (m_entities.empty()) return false;

	return getRootEntity()->findLocalDeclaration(driver, reversePath);
}

bool AST::isEmpty(const hlim::NodeGroup *group, bool reccursive) const
{
	for (auto n : group->getNodes())
		if (isPartOfExport(n)) return false;

	if (reccursive)
		for (const auto &c : group->getChildren())
			if (!isEmpty(c.get(), reccursive)) return false;

	return true;
}


}
