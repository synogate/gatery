#pragma once

#include "ConnectionType.h"

#include "../utils/StackTrace.h"
#include "../utils/LinkedList.h"
#include "../utils/Exceptions.h"

#include <vector>
#include <set>
#include <string>

namespace mhdl::core::hlim {

class Node;
class NodeIO;

struct NodePort {
    Node *node = nullptr;
    size_t port = ~0ull;            
};


class ExplorationList {
    public:
        class iterator {
            public:
                using iterator_category = std::forward_iterator_tag;                

                iterator &operator++();
                bool operator!=(const iterator &rhs) const { MHDL_ASSERT(rhs.m_isEndIterator); return m_isEndIterator || m_openList.empty(); }
                NodePort operator*() { return *m_openList.begin(); }
            protected:
                bool m_isEndIterator;
                enum Mode {
                    ONLY_DIRECT,
                    ONLY_SIGNALS,
                    IGNORE_SIGNALS
                };
                Mode m_mode;

                iterator();
                //iterator(Node *node, 
                
                std::set<Node*> m_closedList;
                std::set<NodePort> m_openList;
                
                friend class ExplorationList;
        };
        
        iterator begin();
        iterator end();
    protected:
        NodeIO &m_nodeIO;
        bool m_ignoreSignals;
};


class NodeIO
{
    public:
        virtual ~NodeIO();
        
        inline size_t getNumInputPorts() const { return m_inputPorts.size(); }
        inline size_t getNumOutputPorts() const { return m_outputPorts.size(); }
        
        NodePort getDriver(size_t inputPort) const;
        NodePort getNonSignalDriver(size_t inputPort) const;
        
        const std::vector<NodePort> &getDirectlyDriven(size_t outputPort) const;
        ExplorationList getSignalsDriven(size_t outputPort) const;
        ExplorationList getNonSignalDriven(size_t outputPort) const;

        inline ConnectionType getOutputConnectionType(size_t outputPort) const { return m_outputPorts[outputPort].connectionType; }
    protected:
        inline void setOutputConnectionType(size_t outputPort, ConnectionType connectionType) { m_outputPorts[outputPort].connectionType = connectionType; }
        
        void connectInput(size_t inputPort, const NodePort &output);
        void disconnectInput(size_t inputPort);

        void resizeInputs(size_t num);
        void resizeOutputs(size_t num);
    private:
        struct OutputPort {
            ConnectionType connectionType;            
            std::vector<NodePort> connections;
        };

        std::vector<NodePort> m_inputPorts;
        std::vector<OutputPort> m_outputPorts;
};


}
