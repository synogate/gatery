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
#include "Block.h"

#include "Entity.h"
#include "AST.h"


namespace hcl::core::vhdl {

Block::Block(Entity *parent, const std::string &desiredName) : BasicBlock(parent->getAST(), parent, &parent->getNamespaceScope())
{
    m_name = m_parent->getNamespaceScope().allocateBlockName(desiredName);
}

Block::~Block()
{

}


void Block::buildFrom(hlim::NodeGroup *nodeGroup)
{
    HCL_ASSERT(nodeGroup->getGroupType() == hlim::NodeGroup::GroupType::AREA);

    m_comment = nodeGroup->getComment();

    // collect all instantiations reccursively, since processes can't do entity instantiations they need to happen here.
    collectInstantiations(nodeGroup, true);

    processifyNodes("default", nodeGroup, false);

    for (auto &childGrp : nodeGroup->getChildren()) {
        if (childGrp->getGroupType() == hlim::NodeGroup::GroupType::AREA)
            processifyNodes(childGrp->getName(), childGrp.get(), true); // reccursively merge all areas into this process(es)
    }
}


void Block::writeVHDL(std::ostream &stream)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    cf.formatBlockComment(stream, m_name, m_comment);
    cf.indent(stream, 1);
    stream << m_name << " : BLOCK" << std::endl;

    for (const auto &signal : m_localSignals) {
        cf.indent(stream, 2);
        stream << "SIGNAL " << m_namespaceScope.getName(signal) << " : ";
        cf.formatConnectionType(stream, hlim::getOutputConnectionType(signal));
        stream << "; "<< std::endl;
    }

    cf.indent(stream, 1);
    stream << "BEGIN" << std::endl;

    writeStatementsVHDL(stream, 2);

    cf.indent(stream, 1);
    stream << "END BLOCK;" << std::endl << std::endl;

}


}
