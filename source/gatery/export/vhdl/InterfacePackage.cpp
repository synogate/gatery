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
#include "InterfacePackage.h"

#include "AST.h"
#include "CodeFormatting.h"

namespace gtry::vhdl {


InterfacePackage::InterfacePackage(AST &ast, const InterfacePackageContent &content) : Package(ast, content.getName()), m_content(content)
{
    HCL_DESIGNCHECK_HINT(m_name == m_content.getName(), "The desired name of the interface package could not be used because it is invalid or clashes with another identifier!");
}

void InterfacePackage::writeVHDL(std::ostream &stream)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    stream << cf.getFileHeader();

    writeLibrariesVHDL(stream);

    stream << "PACKAGE " << m_name << " IS" << std::endl;

	for (const auto &p : m_content.getIntegerConstants()) {
        if (!p.comment.empty()) {
            cf.indent(stream, 1);
            stream << "-- " << p.comment << std::endl;
        }
		cf.indent(stream, 1);
		stream << "constant " << p.name << " : integer := " << p.value << ";" << std::endl;
	}

	for (const auto &p : m_content.getBVecConstants()) {
        if (!p.comment.empty()) {
            cf.indent(stream, 1);
            stream << "-- " << p.comment << std::endl;
        }

		cf.indent(stream, 1);
		if (p.width == 0)
			stream << "constant " << p.name << " : std_logic_vector(-1 downto 0);" << std::endl;
		else
			stream << "constant " << p.name << " : std_logic_vector(" << p.width-1 << " downto 0) := " << p.value << ";" << std::endl;
	}

	for (const auto &p : m_content.getBitConstants()) {
        if (!p.comment.empty()) {
            cf.indent(stream, 1);
            stream << "-- " << p.comment << std::endl;
        }

		cf.indent(stream, 1);
		stream << "constant " << p.name << " : std_logic := " << p.value << ";" << std::endl;
	}

    stream << "END PACKAGE " << m_name << ';' << std::endl << std::endl;

    stream << "PACKAGE BODY " << m_name << " IS" << std::endl;

    stream << "END PACKAGE BODY " << m_name << ';' << std::endl;
}


}
