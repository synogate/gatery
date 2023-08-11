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
#include "HelperPackage.h"

#include "AST.h"
#include "CodeFormatting.h"

namespace gtry::vhdl {


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

	cf.indent(stream, 1);
	stream << "FUNCTION PORTMAP_TO_STDLOGIC(b : BIT) RETURN STD_LOGIC;" << std::endl;
	cf.indent(stream, 1);
	stream << "FUNCTION PORTMAP_TO_STDULOGIC(b : BIT) RETURN STD_ULOGIC;" << std::endl;
	cf.indent(stream, 1);
	stream << "FUNCTION PORTMAP_TO_BIT(v : STD_LOGIC) RETURN BIT;" << std::endl;
	cf.indent(stream, 1);
	stream << "FUNCTION PORTMAP_TO_STDLOGICVECTOR(v : BIT_VECTOR) RETURN STD_LOGIC_VECTOR;" << std::endl;
	cf.indent(stream, 1);
	stream << "FUNCTION PORTMAP_TO_UNSIGNED(v : BIT_VECTOR) RETURN UNSIGNED;" << std::endl;

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

	stream << R"Delim(
	FUNCTION PORTMAP_TO_STDLOGIC(b : BIT) RETURN STD_LOGIC IS
	BEGIN
		IF b THEN
			RETURN '1';
		ELSE
			RETURN '0';
		END IF;
	END PORTMAP_TO_STDLOGIC;

	FUNCTION PORTMAP_TO_STDULOGIC(b : BIT) RETURN STD_ULOGIC IS
	BEGIN
		IF b THEN
			RETURN '1';
		ELSE
			RETURN '0';
		END IF;
	END PORTMAP_TO_STDULOGIC;
	
	FUNCTION PORTMAP_TO_BIT(v : STD_LOGIC) RETURN BIT IS
	BEGIN
		RETURN TO_BIT(v);
	END PORTMAP_TO_BIT;
	
	FUNCTION PORTMAP_TO_STDLOGICVECTOR(v : BIT_VECTOR) RETURN STD_LOGIC_VECTOR IS
	BEGIN
		RETURN TO_STDLOGICVECTOR(v);
	END PORTMAP_TO_STDLOGICVECTOR;

	FUNCTION PORTMAP_TO_UNSIGNED(v : BIT_VECTOR) RETURN UNSIGNED IS
	BEGIN
		RETURN UNSIGNED(TO_STDLOGICVECTOR(v));
	END PORTMAP_TO_UNSIGNED;
)Delim";

	stream << "END PACKAGE BODY " << m_name << ';' << std::endl;
}


}
