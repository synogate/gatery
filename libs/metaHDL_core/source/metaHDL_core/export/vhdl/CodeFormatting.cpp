#include "CodeFormatting.h"

namespace mhdl {
namespace core {
namespace vhdl {


DefaultCodeFormatting::DefaultCodeFormatting()
{ 
    m_indentation = "    ";
    m_fileHeader = 
R"Delim(
--------------------------------------------------------------------
-- This file is under some license that we haven't figured out yet.
-- Also it was auto generated. DO NOT MODIFY. Any changes made
-- directly can not be brought back into the source material and
-- will be lost uppon regeneration.
--------------------------------------------------------------------
)Delim";
}

std::filesystem::path DefaultCodeFormatting::getFilename(const hlim::NodeGroup *nodeGroup) const {
    
    std::filesystem::path path = nodeGroup->getName() + ".vhdl";
    
    const hlim::NodeGroup *parent = nodeGroup->getParent();
    while (parent != nullptr) {
        path = parent->getName() / path;
        parent = parent->getParent();
    }
    
    return path;
}


}
}
}
