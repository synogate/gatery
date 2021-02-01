#pragma once

#include "NodePort.h"
#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

#include <stack>

namespace hcl::core::hlim {

class BaseNode;
class NodeIO;


template<bool forward, typename Policy>
class Exploration {
    public:
        class iterator;

        class NodePortHandle {
            public:
                bool isSignal() const;
                
                template<typename NodeType>
                bool isNodeType() const{ return dynamic_cast<const NodeType*>(node()) != nullptr; }

                bool isBranchingForward();
                bool isBranchingBackward();

                void backtrack();

                BaseNode *node() { return m_nodePort.node; }
                const BaseNode *node() const { return m_nodePort.node; }
                /// When exploring forward the input port, when exploring backwards the output port.
                size_t port() const { return m_nodePort.port; }
                /// When exploring forward the input port, when exploring backwards the output port.
                NodePort nodePort() const { return m_nodePort; }
            protected:
                iterator &m_iterator;
                NodePort m_nodePort;

                NodePortHandle(iterator &iterator, NodePort nodePort);

                friend class iterator;
        };

        class iterator {
            public:
                using iterator_category = std::forward_iterator_tag;                

                iterator() : m_isEndIterator(true) { }
                iterator(bool skipDependencies, NodePort nodePort) : m_skipDependencies(skipDependencies), m_isEndIterator(false) { m_policy.init(nodePort); }

                iterator &operator++() { if (m_ignoreAdvance) m_ignoreAdvance = false; else m_policy.advance(m_skipDependencies); return *this; }
                bool operator!=(const iterator &rhs) const { HCL_ASSERT(rhs.m_isEndIterator); return !m_policy.done(); }
                NodePortHandle operator*() { return NodePortHandle(*this, m_policy.getCurrent()); }
                void backtrack() { m_policy.backtrack(); m_ignoreAdvance = true; }
            protected:
                bool m_skipDependencies;
                bool m_isEndIterator;
                Policy m_policy;
                bool m_ignoreAdvance = false;
        };
        
        Exploration(NodePort nodePort);

        Exploration<forward, Policy> skipDependencies() {
            Exploration<forward, Policy> res(m_nodePort);
            res.m_skipDependencies = true;
            return res;
        }

        iterator begin();
        iterator end();
    protected:
        bool m_skipDependencies = false;
        NodePort m_nodePort;
};

template<bool forward>
class DepthFirstPolicy {
    public:
        void init(NodePort nodePort);
        void advance(bool skipDependencies);
        void backtrack();
        bool done() const;
        NodePort getCurrent();
    protected:
        std::stack<NodePort> m_stack;
};

extern template class DepthFirstPolicy<true>;
extern template class DepthFirstPolicy<false>;

extern template class Exploration<true, DepthFirstPolicy<true>>;
extern template class Exploration<false, DepthFirstPolicy<false>>;

using ExplorationFwdDepthFirst = Exploration<true, DepthFirstPolicy<true>>;
using ExplorationBwdDepthFirst = Exploration<false, DepthFirstPolicy<false>>;

}