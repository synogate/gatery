#pragma once

#include <hcl/frontend/Scope.h>

namespace hcl::stl::buildCtrl {

class TargetVendor : public hcl::core::frontend::BaseScope<TargetVendor>
{
    public:
        enum Vendor {
            VENDOR_GENERIC,
            VENDOR_XILINX,
            VENDOR_ALTERA,
            VENDOR_LATTICE,
        };
        
        TargetVendor(Vendor vendor);
        
        static TargetVendor *get() { return m_currentScope; }
        static Vendor getVendor() { return m_currentScope==nullptr?VENDOR_GENERIC:m_currentScope->m_vendor; }
    protected:
        Vendor m_vendor;
};

}
