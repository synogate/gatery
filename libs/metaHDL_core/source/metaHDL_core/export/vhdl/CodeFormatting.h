#pragma once

#include "../../hlim/NodeGroup.h"

#include <filesystem>

#include <string>

namespace mhdl {
namespace core {
namespace vhdl {


/**
 * @todo write docs
 */
class CodeFormatting
{
    public:
        virtual ~CodeFormatting() = default;
        
        inline const std::string &getIndentation() const { return m_indentation; }
        inline const std::string &getFileHeader() const { return m_fileHeader; }
        
        virtual std::filesystem::path getFilename(const hlim::NodeGroup *nodeGroup) const = 0;
    protected:
        std::string m_indentation;
        std::string m_fileHeader;
};



class DefaultCodeFormatting : public CodeFormatting
{
    public:
        DefaultCodeFormatting();
        
        virtual std::filesystem::path getFilename(const hlim::NodeGroup *nodeGroup) const override;
};


}
}
}
