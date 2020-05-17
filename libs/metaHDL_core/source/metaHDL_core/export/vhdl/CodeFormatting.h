#pragma once

#include "../../hlim/NodeGroup.h"
#include "../../hlim/supportNodes/Node_External.h"

#include <filesystem>

#include <string>
#include <ostream>
#include <functional>

namespace mhdl::core::vhdl {


/**
 * @todo write docs
 */
class CodeFormatting
{
    public:
        virtual ~CodeFormatting() = default;
        
        inline const std::string &getIndentation() const { return m_indentation; }
        inline const std::string &getFileHeader() const { return m_fileHeader; }
        void indent(std::ostream &stream, unsigned depth);
        
        virtual std::filesystem::path getFilename(const hlim::NodeGroup *nodeGroup) const = 0;
        
        enum SignalType {
            SIG_ENTITY_INPUT,
            SIG_ENTITY_OUTPUT,
            SIG_CHILD_ENTITY_INPUT,
            SIG_CHILD_ENTITY_OUTPUT,
            SIG_REGISTER_INPUT,
            SIG_REGISTER_OUTPUT,
            SIG_LOCAL_SIGNAL,
            SIG_LOCAL_VARIABLE,
        };
        
        virtual std::string getNodeName(const hlim::BaseNode *node, unsigned attempt) const = 0;
        virtual std::string getSignalName(const std::string &desiredName, SignalType type, unsigned attempt) const = 0;
        virtual std::string getGlobalName(const std::string &id, unsigned attempt) const = 0;
        
        virtual void instantiateExternal(std::ostream &stream, const hlim::Node_External *node, const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames) const = 0;
    protected:
        std::string m_indentation;
        std::string m_fileHeader;
};



class DefaultCodeFormatting : public CodeFormatting
{
    public:
        using ExternalNodeHandler = std::function<bool(std::ostream &, const hlim::Node_External *, const std::vector<std::string> &, const std::vector<std::string> &)>;
        
        DefaultCodeFormatting();
        
        virtual std::filesystem::path getFilename(const hlim::NodeGroup *nodeGroup) const override;

        virtual std::string getNodeName(const hlim::BaseNode *node, unsigned attempt) const override;
        virtual std::string getSignalName(const std::string &desiredName, SignalType type, unsigned attempt) const override;
        virtual std::string getGlobalName(const std::string &id, unsigned attempt) const override;

        void addExternalNodeHandler(ExternalNodeHandler nodeHandler);
        virtual void instantiateExternal(std::ostream &stream, const hlim::Node_External *node, const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames) const override;
    protected:
        std::vector<ExternalNodeHandler> m_externalNodeHandlers;
};


}
