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

#include "NamespaceScope.h"

#include <vector>
#include <memory>

namespace gtry::vhdl {
    
class Entity;
class Process;
class Block;


/**
 * @todo write docs
 */
class BaseGrouping
{
    public:
        BaseGrouping(AST &ast, BaseGrouping *parent, NamespaceScope *parentNamespace);
        virtual ~BaseGrouping();
        
        inline AST &getAST() { return m_ast; }
        inline NamespaceScope &getNamespaceScope() { return m_namespaceScope; }
        inline BaseGrouping *getParent() { return m_parent; }
        inline const BaseGrouping *getParent() const { return m_parent; }
        bool isChildOf(const BaseGrouping *other) const;
        
        virtual void extractSignals() = 0;
        virtual void allocateNames() = 0;

        inline const std::set<hlim::NodePort> &getLocalSignals() { return m_localSignals; }
        inline const std::set<hlim::NodePort> &getInputs() { return m_inputs; }
        inline const std::set<hlim::NodePort> &getOutputs() { return m_outputs; }
        inline const std::set<hlim::Clock *> &getClocks() { return m_inputClocks; }
        inline const std::set<hlim::Node_Pin *> &getIoPins() { return m_ioPins; }
    protected:
        AST &m_ast;
        NamespaceScope m_namespaceScope;
        BaseGrouping *m_parent;
        std::string m_name;
        std::string m_comment;
        
        std::set<hlim::NodePort> m_localSignals;
        std::set<hlim::NodePort> m_inputs;
        std::set<hlim::NodePort> m_outputs;
        std::set<hlim::Clock *> m_inputClocks;
        std::set<hlim::Node_Pin*> m_ioPins;
        
        
        bool isProducedExternally(hlim::NodePort nodePort);
        bool isConsumedExternally(hlim::NodePort nodePort);
        
        std::string findNearestDesiredName(hlim::NodePort nodePort);

        void verifySignalsDisjoint();
};


}
