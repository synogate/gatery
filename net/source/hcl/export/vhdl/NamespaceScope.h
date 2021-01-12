#pragma once

#include "../../hlim/Node.h"
#include "../../hlim/SignalGroup.h"

#include "CodeFormatting.h"

#include <boost/algorithm/string.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>


namespace hcl::core::vhdl {

class AST;

struct NodeInternalStorageSignal
{
    hlim::BaseNode *node;
    size_t signalIdx;

    inline bool operator==(const NodeInternalStorageSignal &rhs) const { return node == rhs.node && signalIdx == rhs.signalIdx; }
    inline bool operator<(const NodeInternalStorageSignal &rhs) const { if (node < rhs.node) return true; if (node > rhs.node) return false; return signalIdx < rhs.signalIdx; }
};


struct TypeDefinition
{
    std::vector<hlim::SignalGroup*> signalGroups;
    std::string typeName;
    std::string desiredTypeName;

    bool compatibleWith(hlim::SignalGroup *signalGroup);
};


/**
 * @todo write docs
 */
class NamespaceScope
{
    public:
        NamespaceScope(AST &ast, NamespaceScope *parent);
        virtual ~NamespaceScope() { }

        std::string allocateName(hlim::NodePort nodePort, const std::string &desiredName, CodeFormatting::SignalType type);
        const std::string &getName(hlim::NodePort nodePort) const;

        std::string allocateName(NodeInternalStorageSignal nodePort, const std::string &desiredName);
        const std::string &getName(NodeInternalStorageSignal nodePort) const;

        std::string allocateName(hlim::Clock *clock, const std::string &desiredName);
        const std::string &getName(hlim::Clock *clock) const;

        std::string allocateName(hlim::Node_Pin *ioPin, const std::string &desiredName);
        const std::string &getName(hlim::Node_Pin *ioPin) const;

        std::string allocateEntityName(const std::string &desiredName);
        std::string allocateBlockName(const std::string &desiredName);
        std::string allocateProcessName(const std::string &desiredName, bool clocked);
    protected:
        bool isNameInUse(const std::string &upperCaseName) const;
        AST &m_ast;
        NamespaceScope *m_parent;
        
        std::set<std::string> m_namesInUse;
        std::map<hlim::NodePort, std::string> m_nodeNames;
        std::map<NodeInternalStorageSignal, std::string> m_nodeStorageNames;
        std::map<hlim::Clock*, std::string> m_clockNames;
        std::map<hlim::Node_Pin*, std::string> m_ioPinNames;
        std::vector<TypeDefinition> m_typeDefinitions;
};


}
