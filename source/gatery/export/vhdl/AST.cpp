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
#include "../../hlim/NodeGroup.h"

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
	m_exportArea.addAllForExport(circuit);

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

std::filesystem::path AST::getFilename(std::filesystem::path basePath, const std::string &name)
{
	return basePath / (name + m_codeFormatting->getFilenameExtension());
}

void AST::writeVHDL(std::filesystem::path destination)
{
	if (destination.has_extension())
	{
		for (auto& entity : m_entities)
			entity->writeSupportFiles(destination.parent_path());

		std::ofstream file{ destination.c_str(), std::ofstream::binary };
		file.exceptions(std::fstream::failbit | std::fstream::badbit);

		for (auto& package : m_packages)
			package->writeVHDL(file);

		for (auto* entity : this->getDependencySortedEntities())
			entity->writeVHDL(file);
	}
	else
	{
		for (auto& entity : m_entities)
			entity->writeSupportFiles(destination);

		for (auto& package : m_packages) {
			std::filesystem::path filePath = getFilename(destination, package->getName());

			std::fstream file(filePath.string().c_str(), std::fstream::out);
			file.exceptions(std::fstream::failbit | std::fstream::badbit);
			package->writeVHDL(file);
		}

		for (auto& entity : m_entities) {
			std::filesystem::path filePath = getFilename(destination, entity->getName());

			std::fstream file(filePath.string().c_str(), std::fstream::out);
			file.exceptions(std::fstream::failbit | std::fstream::badbit);
			entity->writeVHDL(file);
		}
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
