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

#include "BasicBlock.h"

#include <string>
#include <vector>
#include <memory>
#include <ostream>

namespace gtry::vhdl {

class Block;

/**
 * @todo write docs
 */
class Entity : public BasicBlock
{
    public:
        Entity(AST &ast, const std::string &desiredName, BasicBlock *parent);
        virtual ~Entity();

        inline const std::string &getName() const { return m_name; }

        void buildFrom(hlim::NodeGroup *nodeGroup);

        virtual void extractSignals() override;
        virtual void allocateNames() override;

        void writeVHDL(std::ostream &stream);

        void writePortDeclaration(std::ostream &stream, size_t indentation);

        virtual void writeInstantiationVHDL(std::ostream &stream, unsigned indent, const std::string &instanceName);

        Entity *getParentEntity();

        virtual bool findLocalDeclaration(hlim::NodePort driver, std::vector<BaseGrouping*> &reversePath) override;

        inline const std::vector<std::unique_ptr<Block>> &getBlocks() const { return m_blocks; }

        virtual std::string getInstanceName() override;

        std::set<std::string> collectNeededLibraries();
    protected:
        std::vector<std::unique_ptr<Block>> m_blocks;

        virtual void writeLibrariesVHDL(std::ostream &stream);
        virtual std::vector<std::string> getPortsVHDL();
        virtual void writeLocalSignalsVHDL(std::ostream &stream);
};

}
