#pragma once

#include "Package.h"

namespace hcl::core::vhdl {

class HelperPackage : public Package {
    public:
        HelperPackage(AST &ast);

        virtual void writeVHDL(std::ostream &stream) override;
    protected:
};

}