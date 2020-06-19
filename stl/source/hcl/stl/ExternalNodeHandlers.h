#pragma once

#include "blockRam/XilinxSimpleDualPortBlockRam.h"

namespace hcl::stl {
    
template<class CodeFormatter>
void registerExternalNodeHandlerVHDL(CodeFormatter *codeFormatter) 
{
    codeFormatter->addExternalNodeHandler(XilinxSimpleDualPortBlockRam::writeVHDL);
}
    
}
