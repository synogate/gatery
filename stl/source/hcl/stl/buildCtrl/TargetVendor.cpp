#include "TargetVendor.h"

namespace hcl::stl::buildCtrl {

TargetVendor::TargetVendor(Vendor vendor) : BaseScope<TargetVendor>()
{
    m_vendor = vendor;
}

}
