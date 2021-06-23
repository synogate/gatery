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
#pragma once

#include "NamespaceScope.h"

#include <filesystem>

#include <vector>
#include <memory>
#include <string>

namespace gtry {
    class SynthesisTool;
}

namespace gtry::hlim {
    class Circuit;
    class BaseNode;
}

namespace gtry::vhdl {

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
        AST(CodeFormatting *codeFormatting, SynthesisTool *synthesisTool);
        ~AST();

        void convert(hlim::Circuit &circuit);

        Entity &createEntity(const std::string &desiredName, BasicBlock *parent);

        template<typename Type, typename... Args>
        Type &createSpecialEntity(Args&&... args) {
            m_entities.push_back(std::make_unique<Type>(*this, std::forward<Args>(args)...));
            return (Type&)*m_entities.back();
        }

        inline CodeFormatting &getCodeFormatting() { return *m_codeFormatting; }
        inline SynthesisTool &getSynthesisTool() { return *m_synthesisTool; }
        inline NamespaceScope &getNamespaceScope() { return m_namespaceScope; }
        inline Hlim2AstMapping &getMapping() { return m_mapping; }

        void writeVHDL(std::filesystem::path destination);

        std::filesystem::path getFilename(std::filesystem::path basePath, const std::string &name);

        inline const std::vector<std::unique_ptr<Entity>> &getEntities() { return m_entities; }
        inline const std::vector<std::unique_ptr<Package>> &getPackages() { return m_packages; }

        inline Entity *getRootEntity() { return m_entities.front().get(); }
        inline const Entity *getRootEntity() const { return m_entities.front().get(); }

        bool findLocalDeclaration(hlim::NodePort driver, std::vector<BaseGrouping*> &reversePath);

        std::vector<Entity*> getDependencySortedEntities();
    protected:
        CodeFormatting *m_codeFormatting;
        SynthesisTool *m_synthesisTool;
        NamespaceScope m_namespaceScope;
        std::vector<std::unique_ptr<Entity>> m_entities;
        std::vector<std::unique_ptr<Package>> m_packages;
        Hlim2AstMapping m_mapping;

};

}
