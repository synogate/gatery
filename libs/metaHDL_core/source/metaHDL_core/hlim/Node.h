#pragma once

#include "ConnectionType.h"

#include "../utils/StackTrace.h"
#include "../utils/LinkedList.h"
#include "../utils/Exceptions.h"

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
        Node(NodeGroup *group, size_t numInputs, size_t numOutputs);
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
        
        virtual std::string getTypeName() const = 0;
        virtual void assertValidity() const = 0;
        virtual std::string getInputName(size_t idx) const = 0;
        virtual std::string getOutputName(size_t idx) const = 0;
        
        inline void recordStackTrace() { m_stackTrace.record(10, 1); }
        inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }

        inline void setName(std::string name) { m_name = std::move(name); }
        inline const std::string &getName() const { return m_name; }
        
        size_t getNumInputs() const { return m_inputs.size(); }
        InputPort &getInput(size_t index) { return m_inputs[index]; }
        const InputPort &getInput(size_t index) const { return m_inputs[index]; }
        size_t getNumOutputs() const { return m_outputs.size(); }
        OutputPort &getOutput(size_t index) { return m_outputs[index]; }
        const OutputPort &getOutput(size_t index) const { return m_outputs[index]; }

        bool isOrphaned() const;
        const NodeGroup *getGroup() const { return m_nodeGroup; }
        
        void moveToGroup(NodeGroup *group);
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
