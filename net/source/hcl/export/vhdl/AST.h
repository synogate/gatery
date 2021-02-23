#pragma once

#include "NamespaceScope.h"

#include <filesystem>

#include <vector>
#include <memory>
#include <string>

namespace hcl::core::hlim {
    class Circuit;
    class BaseNode;
}

namespace hcl::core::vhdl {

class Entity;
class BaseGrouping;
class BasicBlock;
class CodeFormatting;
class Package;

class Hlim2AstMapping
{
    public:
        void assignNodeToScope(hlim::BaseNode *node, BaseGrouping *scope);
        BaseGrouping *getScope(hlim::BaseNode *node) const;
    protected:
        std::map<hlim::BaseNode*, BaseGrouping*> m_node2Block;
};


class AST
{
    public:
        AST(CodeFormatting *codeFormatting);
        ~AST();

        void convert(hlim::Circuit &circuit);

        Entity &createEntity(const std::string &desiredName, BasicBlock *parent);

        template<typename Type, typename... Args>
        Type &createSpecialEntity(Args&&... args) {
            m_entities.push_back(std::make_unique<Type>(*this, std::forward<Args>(args)...));
            return (Type&)*m_entities.back();
        }

        inline CodeFormatting &getCodeFormatting() { return *m_codeFormatting; }
        inline NamespaceScope &getNamespaceScope() { return m_namespaceScope; }
        inline Hlim2AstMapping &getMapping() { return m_mapping; }

        void writeVHDL(std::filesystem::path destination);

        std::filesystem::path getFilename(std::filesystem::path basePath, const std::string &name);

        inline const std::vector<std::unique_ptr<Entity>> &getEntities() { return m_entities; }
        inline const std::vector<std::unique_ptr<Package>> &getPackages() { return m_packages; }

        inline Entity *getRootEntity() { return m_entities.front().get(); }

        std::vector<Entity*> getDependencySortedEntities();
    protected:
        CodeFormatting *m_codeFormatting;
        NamespaceScope m_namespaceScope;
        std::vector<std::unique_ptr<Entity>> m_entities;
        std::vector<std::unique_ptr<Package>> m_packages;
        Hlim2AstMapping m_mapping;

};

}
