#pragma once

#include "NamespaceScope.h"

#include <vector>
#include <memory>

namespace mhdl::core::vhdl {
    
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
        inline const std::set<hlim::BaseClock *> &getClocks() { return m_inputClocks; }
    protected:
        AST &m_ast;
        NamespaceScope m_namespaceScope;
        BaseGrouping *m_parent;
        std::string m_name;
        std::string m_comment;
        
        std::set<hlim::NodePort> m_localSignals;
        std::set<hlim::NodePort> m_inputs;
        std::set<hlim::NodePort> m_outputs;
        std::set<hlim::BaseClock *> m_inputClocks;
        
        
        bool isProducedExternally(hlim::NodePort nodePort);
        bool isConsumedExternally(hlim::NodePort nodePort);
        
        std::string findNearestDesiredName(hlim::NodePort nodePort);
};


}
