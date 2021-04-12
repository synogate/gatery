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
#pragma once

#include <hcl/frontend/Scope.h>

namespace hcl::scl::buildCtrl {

class TargetVendor : public hcl::BaseScope<TargetVendor>
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
