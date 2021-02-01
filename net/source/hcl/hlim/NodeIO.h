#pragma once

#include "NodePort.h"
#include "ConnectionType.h"
#include "GraphExploration.h"
#include "../simulation/BitVectorState.h"

#include "../utils/StackTrace.h"
#include "../utils/LinkedList.h"
#include "../utils/Exceptions.h"

#include <vector>
#include <set>
#include <string>

namespace hcl::core::hlim {

class BaseNode;
class NodeIO;
class Circuit;

class NodeIO
{
    public:
        enum OutputType {
            OUTPUT_IMMEDIATE,
            OUTPUT_LATCHED,
            // latched and immediate
            OUTPUT_CONSTANT
        };

        virtual ~NodeIO();
        
        inline size_t getNumInputPorts() const { return m_inputPorts.size(); }
        inline size_t getNumOutputPorts() const { return m_outputPorts.size(); }
        
        ConnectionType getDriverConnType(size_t inputPort) const;
        NodePort getDriver(size_t inputPort) const;
        NodePort getNonSignalDriver(size_t inputPort) const;

        const std::vector<NodePort> &getDirectlyDriven(size_t outputPort) const;

        ExplorationFwdDepthFirst exploreOutput(size_t port) { return ExplorationFwdDepthFirst({.node=(BaseNode*)this, .port = port}); }
        ExplorationBwdDepthFirst exploreInput(size_t port) { return ExplorationBwdDepthFirst({.node=(BaseNode*)this, .port = port}); }

        inline const ConnectionType &getOutputConnectionType(size_t outputPort) const { return m_outputPorts[outputPort].connectionType; }
        inline OutputType getOutputType(size_t outputPort) const { return m_outputPorts[outputPort].outputType; }
        
        void bypassOutputToInput(size_t outputPort, size_t inputPort);

        inline void rewireInput(size_t inputPort, const NodePort &output) { connectInput(inputPort, output); }
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
