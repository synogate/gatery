#include "CodeFormatting.h"

#include <boost/format.hpp>

namespace mhdl::core::vhdl {


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

std::string DefaultCodeFormatting::getNodeName(const hlim::Node *node, unsigned attempt) const
{
    std::string initialName = node->getName();
    if (initialName.empty())
        initialName = "unnamed";
    if (attempt == 0)
        return initialName;
    
    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getGlobalName(const std::string &id, unsigned attempt) const
{
    if (attempt == 0)
        return id;
    
    return (boost::format("%s_%d") % id % (attempt+1)).str();
}


}
