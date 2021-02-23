#include "AST.h"

#include "Entity.h"
#include "CodeFormatting.h"
#include "HelperPackage.h"

#include "../../hlim/Circuit.h"

#include <fstream>

namespace hcl::core::vhdl {


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

}
