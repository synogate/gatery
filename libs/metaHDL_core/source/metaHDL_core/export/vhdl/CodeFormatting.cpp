#include "CodeFormatting.h"

#include "../../utils/Range.h"

#include <boost/format.hpp>

namespace mhdl::core::vhdl {


void CodeFormatting::indent(std::ostream &stream, unsigned depth)
{
    for (auto i : utils::Range(depth))
        stream << m_indentation;
}
    
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

std::string DefaultCodeFormatting::getNodeName(const hlim::BaseNode *node, unsigned attempt) const
{
    std::string initialName = node->getName();
    if (initialName.empty())
        initialName = "unnamed";
    if (attempt == 0)
        return initialName;
    
    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getSignalName(const std::string &desiredName, SignalType type, unsigned attempt) const 
{
    std::string initialName = desiredName;
    if (initialName.empty())
        initialName = "unnamed";
    
    switch (type) {
        case SIG_ENTITY_INPUT: initialName = std::string("in_") + initialName; break;
        case SIG_ENTITY_OUTPUT: initialName = std::string("out_") + initialName; break;
        case SIG_CHILD_ENTITY_INPUT: initialName = std::string("c_in_") + initialName; break;
        case SIG_CHILD_ENTITY_OUTPUT: initialName = std::string("c_out_") + initialName; break;
        case SIG_REGISTER_INPUT: initialName = std::string("r_in_") + initialName; break;
        case SIG_REGISTER_OUTPUT: initialName = std::string("r_out_") + initialName; break;
        case SIG_LOCAL_SIGNAL: initialName = std::string("s_") + initialName; break;
        case SIG_LOCAL_VARIABLE: initialName = std::string("v_") + initialName; break;
    }
    
    if (attempt == 0)
        return initialName;
    
    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getGlobalName(const std::string &id, unsigned attempt) const
{
    std::string initialName = id;
    if (initialName.empty())
        initialName = "unnamed";
    if (attempt == 0)
        return initialName;
    
    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}


void DefaultCodeFormatting::addExternalNodeHandler(ExternalNodeHandler nodeHandler)
{
    m_externalNodeHandlers.push_back(std::move(nodeHandler));
}

void DefaultCodeFormatting::instantiateExternal(std::ostream &stream, const hlim::Node_External *node, const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames) const
{
    for (const auto &handler : m_externalNodeHandlers)
        if (handler(stream, node, inputSignalNames, outputSignalNames))
            return;
}


}
