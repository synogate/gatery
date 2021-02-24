#include "Package.h"

#include "AST.h"

namespace hcl::core::vhdl {

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