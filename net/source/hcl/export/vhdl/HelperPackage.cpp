#include "HelperPackage.h"

#include "AST.h"
#include "CodeFormatting.h"

namespace hcl::core::vhdl {


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