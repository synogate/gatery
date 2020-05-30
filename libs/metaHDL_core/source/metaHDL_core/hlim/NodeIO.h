#pragma once

#include "ConnectionType.h"
#include "../simulation/BitVectorState.h"

#include "../utils/StackTrace.h"
#include "../utils/LinkedList.h"
#include "../utils/Exceptions.h"

#include <vector>
#include <set>
#include <string>

namespace mhdl::core::hlim {

class BaseNode;
class NodeIO;

constexpr size_t INV_PORT = std::numeric_limits<std::size_t>::max();

struct NodePort {
    BaseNode *node = nullptr;
    size_t port = INV_PORT;
    
    inline bool operator==(const NodePort &rhs) const { return node == rhs.node && port == rhs.port; }
    inline bool operator<(const NodePort &rhs) const { if (node < rhs.node) return true; if (node > rhs.node) return false; return port < rhs.port; }
};

/*
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
                
                std::set<BaseNode*> m_closedList;
                std::set<NodePort> m_openList;
                
                friend class ExplorationList;
        };
        
        iterator begin();
        iterator end();
    protected:
        NodeIO &m_nodeIO;
        bool m_ignoreSignals;
};
*/
class Circuit;

class NodeIO
{
    public:
        enum OutputType {
            OUTPUT_IMMEDIATE,
            OUTPUT_LATCHED,
            OUTPUT_CONSTANT
        };

        virtual ~NodeIO();
        
        inline size_t getNumInputPorts() const { return m_inputPorts.size(); }
        inline size_t getNumOutputPorts() const { return m_outputPorts.size(); }
        
        NodePort getDriver(size_t inputPort) const;
        NodePort getNonSignalDriver(size_t inputPort) const;
        
        const std::vector<NodePort> &getDirectlyDriven(size_t outputPort) const;
        /*
        ExplorationList getSignalsDriven(size_t outputPort) const;
        ExplorationList getNonSignalDriven(size_t outputPort) const;
        */

        inline const ConnectionType &getOutputConnectionType(size_t outputPort) const { return m_outputPorts[outputPort].connectionType; }
        inline OutputType getOutputType(size_t outputPort) const { return m_outputPorts[outputPort].outputType; }
    protected:
        void setOutputConnectionType(size_t outputPort, const ConnectionType &connectionType);
        void setOutputType(size_t outputPort, OutputType outputType);
        
        void connectInput(size_t inputPort, const NodePort &output);
        void disconnectInput(size_t inputPort);

        void resizeInputs(size_t num);
        void resizeOutputs(size_t num);
    private:
        struct OutputPort {
            ConnectionType connectionType; ///@todo: turn into pointer and cache somewhere
            OutputType outputType = OUTPUT_IMMEDIATE;
            sim::DefaultBitVectorState outputValue;
            std::vector<NodePort> connections;
        };

        std::vector<NodePort> m_inputPorts;
        std::vector<OutputPort> m_outputPorts;

        friend class Circuit;
};


}
