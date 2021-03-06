/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "../../hlim/Node.h"
#include "../../hlim/SignalGroup.h"

#include "CodeFormatting.h"

#include <boost/algorithm/string.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>


namespace gtry::vhdl {

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
        const std::string &getName(const hlim::NodePort nodePort) const;

        std::string allocateName(NodeInternalStorageSignal nodePort, const std::string &desiredName);
        const std::string &getName(NodeInternalStorageSignal nodePort) const;

        std::string allocateName(hlim::Clock *clock, const std::string &desiredName);
        const std::string &getName(const hlim::Clock *clock) const;

        std::string allocateName(hlim::Node_Pin *ioPin, const std::string &desiredName);
        const std::string &getName(const hlim::Node_Pin *ioPin) const;

        std::string allocatePackageName(const std::string &desiredName);
        std::string allocateEntityName(const std::string &desiredName);
        std::string allocateBlockName(const std::string &desiredName);
        std::string allocateProcessName(const std::string &desiredName, bool clocked);
        std::string allocateInstanceName(const std::string &desiredName);
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
