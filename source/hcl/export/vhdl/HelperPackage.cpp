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
#include "HelperPackage.h"

#include "AST.h"
#include "CodeFormatting.h"

namespace hcl::vhdl {


HelperPackage::HelperPackage(AST &ast) : Package(ast, "GateryHelperPackage")
{
}

void HelperPackage::writeVHDL(std::ostream &stream)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    stream << cf.getFileHeader();

    writeLibrariesVHDL(stream);

    stream << "PACKAGE " << m_name << " IS" << std::endl;

    cf.indent(stream, 1);
    stream << "FUNCTION bool2stdlogic(v : BOOLEAN) RETURN STD_LOGIC;" << std::endl;

    cf.indent(stream, 1);
    stream << "FUNCTION stdlogic2bool(v : STD_LOGIC) RETURN BOOLEAN;" << std::endl;

    stream << "END PACKAGE " << m_name << ';' << std::endl << std::endl;

    stream << "PACKAGE BODY " << m_name << " IS" << std::endl;

    cf.indent(stream, 1);
    stream << "FUNCTION bool2stdlogic(v : BOOLEAN) RETURN STD_LOGIC IS" << std::endl;
    cf.indent(stream, 1);
    stream << "BEGIN" << std::endl;
        cf.indent(stream, 2);
        stream << "IF v THEN" << std::endl;
            cf.indent(stream, 3);
            stream << "RETURN '1';" << std::endl;
        cf.indent(stream, 2);
        stream << "ELSE" << std::endl;
            cf.indent(stream, 3);
            stream << "RETURN '0';" << std::endl;
        cf.indent(stream, 2);
        stream << "END IF;" << std::endl;
    cf.indent(stream, 1);
    stream << "END bool2stdlogic;" << std::endl << std::endl;

    cf.indent(stream, 1);
    stream << "FUNCTION stdlogic2bool(v : STD_LOGIC) RETURN BOOLEAN IS" << std::endl;
    cf.indent(stream, 1);
    stream << "BEGIN" << std::endl;
        cf.indent(stream, 2);
        stream << "RETURN v = '1';" << std::endl;
    cf.indent(stream, 1);
    stream << "END stdlogic2bool;" << std::endl << std::endl;

    stream << "END PACKAGE BODY " << m_name << ';' << std::endl;
}


}
