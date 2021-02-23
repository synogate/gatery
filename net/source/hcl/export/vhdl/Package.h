#pragma once

#include <ostream>

namespace hcl::core::vhdl {

class AST;

class Package {
    public:
        Package(AST &ast, const std::string &desiredName);
        virtual ~Package() = default;

        inline const std::string &getName() const { return m_name; }

        virtual void writeVHDL(std::ostream &stream) = 0;

        virtual void writeImportStatement(std::ostream &stream) const;
    protected:
        AST &m_ast;
        std::string m_name;

        virtual void writeLibrariesVHDL(std::ostream &stream); // todo: proper dependency tracking?

};

}