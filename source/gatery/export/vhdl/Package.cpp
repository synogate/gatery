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
#include "Package.h"

#include "AST.h"

namespace gtry::vhdl {

Package::Package(AST &ast, const std::string &desiredName) : m_ast(ast)
{
    m_name = m_ast.getNamespaceScope().allocateEntityName(desiredName);
}


void Package::writeLibrariesVHDL(std::ostream &stream)
{
    stream << "LIBRARY ieee;" << std::endl
           << "USE ieee.std_logic_1164.ALL;" << std::endl
           << "USE ieee.numeric_std.all;" << std::endl << std::endl;
}

void Package::writeImportStatement(std::ostream &stream) const
{
    stream << "LIBRARY work;" << std::endl
           << "USE work."<<m_name<<".all;" << std::endl << std::endl;
}


}
