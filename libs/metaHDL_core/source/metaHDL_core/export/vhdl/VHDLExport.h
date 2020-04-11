#pragma once

#include "CodeFormatting.h"

#include "../../hlim/Circuit.h"

#include <filesystem>
#include <memory>

namespace mhdl {
namespace core {
namespace vhdl {

/**
 * @todo write docs
 */
class VHDLExport
{
    public:
        VHDLExport(std::filesystem::path destination);
        
        VHDLExport &setFormatting(CodeFormatting *codeFormatting);
        
        void operator()(const hlim::Circuit &circuit);        
    protected:
        std::filesystem::path m_destination;
        std::unique_ptr<CodeFormatting> m_codeFormatting;
        
        void exportGroup(const hlim::NodeGroup *group);
};

}
}
}
