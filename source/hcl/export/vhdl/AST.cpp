/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "hcl/pch.h"
#include "AST.h"

#include "Entity.h"
#include "CodeFormatting.h"
#include "HelperPackage.h"
#include "Block.h"

#include "../../hlim/Circuit.h"

#include <fstream>
#include <functional>

namespace hcl::vhdl {


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


AST::AST(CodeFormatting *codeFormatting) : m_codeFormatting(codeFormatting), m_namespaceScope(*this, nullptr)
{
    m_packages.push_back(std::make_unique<HelperPackage>(*this));
}

AST::~AST()
{
}

void AST::convert(hlim::Circuit &circuit)
{
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
    std::filesystem::create_directories(destination);

    for (auto &package : m_packages) {
        std::filesystem::path filePath = getFilename(destination, package->getName());

        std::fstream file(filePath.string().c_str(), std::fstream::out);
        file.exceptions(std::fstream::failbit | std::fstream::badbit);
        package->writeVHDL(file);
    }

    for (auto &entity : m_entities) {
        std::filesystem::path filePath = getFilename(destination, entity->getName());

        std::fstream file(filePath.string().c_str(), std::fstream::out);
        file.exceptions(std::fstream::failbit | std::fstream::badbit);
        entity->writeVHDL(file);
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

}
