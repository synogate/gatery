#pragma once

#include "../../hlim/NodeGroup.h"
#include "../../hlim/supportNodes/Node_External.h"

#include <filesystem>

#include <string>
#include <ostream>
#include <functional>

namespace hcl::core::vhdl {


/**
 * @todo write docs
 */
class CodeFormatting
{
    public:
        virtual ~CodeFormatting() = default;

        inline const std::string &getIndentation() const { return m_indentation; }
        inline const std::string &getFileHeader() const { return m_fileHeader; }
        inline const std::string &getFilenameExtension() const { return m_filenameExtension; }
        void indent(std::ostream &stream, unsigned depth) const;
        virtual void formatEntityComment(std::ostream &stream, const std::string &entityName, const std::string &comment) = 0;
        virtual void formatBlockComment(std::ostream &stream, const std::string &blockName, const std::string &comment) = 0;
        virtual void formatProcessComment(std::ostream &stream, unsigned indentation, const std::string &processName, const std::string &comment) = 0;
        virtual void formatCodeComment(std::ostream &stream, unsigned indentation, const std::string &comment) = 0;

        virtual void formatConnectionType(std::ostream &stream, const hlim::ConnectionType &connectionType) = 0;

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
        virtual std::string getPackageName(const std::string &desiredName, unsigned attempt) const = 0;
        virtual std::string getEntityName(const std::string &desiredName, unsigned attempt) const = 0;
        virtual std::string getBlockName(const std::string &desiredName, unsigned attempt) const = 0;
        virtual std::string getProcessName(const std::string &desiredName, bool clocked, unsigned attempt) const = 0;
        virtual std::string getClockName(const std::string &desiredName, unsigned attempt) const = 0;
        virtual std::string getIoPinName(const std::string &desiredName, unsigned attempt) const = 0;

        virtual void instantiateExternal(std::ostream &stream, const hlim::Node_External *node, unsigned indent,
                                         const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames, const std::vector<std::string> &clockNames) const = 0;
    protected:
        std::string m_indentation;
        std::string m_fileHeader;
        std::string m_filenameExtension;
};



class DefaultCodeFormatting : public CodeFormatting
{
    public:
        using ExternalNodeHandler = std::function<bool(const CodeFormatting*, std::ostream &, const hlim::Node_External *, unsigned, const std::vector<std::string> &, const std::vector<std::string> &, const std::vector<std::string> &)>;

        DefaultCodeFormatting();

        virtual std::string getNodeName(const hlim::BaseNode *node, unsigned attempt) const override;
        virtual std::string getSignalName(const std::string &desiredName, SignalType type, unsigned attempt) const override;
        virtual std::string getPackageName(const std::string &desiredName, unsigned attempt) const override;
        virtual std::string getEntityName(const std::string &desiredName, unsigned attempt) const override;
        virtual std::string getBlockName(const std::string &desiredName, unsigned attempt) const override;
        virtual std::string getProcessName(const std::string &desiredName, bool clocked, unsigned attempt) const override;
        virtual std::string getClockName(const std::string &desiredName, unsigned attempt) const override;
        virtual std::string getIoPinName(const std::string &desiredName, unsigned attempt) const override;

        virtual void formatEntityComment(std::ostream &stream, const std::string &entityName, const std::string &comment) override;
        virtual void formatBlockComment(std::ostream &stream, const std::string &blockName, const std::string &comment) override;
        virtual void formatProcessComment(std::ostream &stream, unsigned indentation, const std::string &processName, const std::string &comment) override;
        virtual void formatCodeComment(std::ostream &stream, unsigned indentation, const std::string &comment) override;

        virtual void formatConnectionType(std::ostream &stream, const hlim::ConnectionType &connectionType) override;

        void addExternalNodeHandler(ExternalNodeHandler nodeHandler);
        virtual void instantiateExternal(std::ostream &stream, const hlim::Node_External *node, unsigned indent,
                                         const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames, const std::vector<std::string> &clockNames) const override;
    protected:
        std::vector<ExternalNodeHandler> m_externalNodeHandlers;
};


}
