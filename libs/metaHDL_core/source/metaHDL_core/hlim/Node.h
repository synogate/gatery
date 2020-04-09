#pragma once

#include "ConnectionType.h"

#include "../utils/StackTrace.h"
#include "../utils/LinkedList.h"

#include <vector>
#include <set>
#include <string>

namespace mhdl {
namespace core {    
namespace hlim {

class NodeGroup;
    
class Node
{
    public:
        Node(NodeGroup *group, unsigned numInputs, unsigned numOutputs);
        virtual ~Node() { }
        
        struct Port {
            Node *node;
            Port(Node *n) : node(n) { }
        };
        struct OutputPort;
        struct InputPort : public Port {
            OutputPort *connection = nullptr;
            InputPort(Node *n) : Port(n) { }
            ~InputPort();
        };
        struct OutputPort : public Port {
            std::set<InputPort*> connections;

            OutputPort(Node *n) : Port(n) { }
            ~OutputPort();
            void connect(InputPort &port);
            void disconnect(InputPort &port);
        };
        
        virtual std::string getTypeName() = 0;
        virtual void assertValidity() = 0;
        virtual std::string getInputName(unsigned idx) = 0;
        virtual std::string getOutputName(unsigned idx) = 0;
        
        inline void recordStackTrace() { m_stackTrace.record(10, 1); }
        inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }

        inline void setName(std::string name) { m_name = std::move(name); }
        
        InputPort &getInput(unsigned index) { return m_inputs[index]; }
        OutputPort &getOutput(unsigned index) { return m_outputs[index]; }

        bool isOrphaned() const;
    protected:
        std::string m_name;
        std::vector<InputPort> m_inputs;
        std::vector<OutputPort> m_outputs;
        utils::StackTrace m_stackTrace;
        NodeGroup *m_nodeGroup;
        
};

}
}
}
