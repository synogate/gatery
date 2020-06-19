#include "NodeIO.h"

#include "Node.h"

#include "coreNodes/Node_Signal.h"

#include "../utils/Range.h"

#include <algorithm>

namespace hcl::core::hlim {

NodeIO::~NodeIO()
{
    resizeInputs(0);
    resizeOutputs(0);
}

NodePort NodeIO::getDriver(size_t inputPort) const
{
    HCL_ASSERT(inputPort < m_inputPorts.size());
    return m_inputPorts[inputPort];
}

NodePort NodeIO::getNonSignalDriver(size_t inputPort) const
{
    NodePort np = m_inputPorts[inputPort];
    while (np.node != nullptr && dynamic_cast<Node_Signal*>(np.node) != nullptr) 
        np = np.node->m_inputPorts[0];
    return np;
}

const std::vector<NodePort> &NodeIO::getDirectlyDriven(size_t outputPort) const
{
    return m_outputPorts[outputPort].connections;
}
/*
ExplorationList NodeIO::getSignalsDriven(size_t outputPort) const
{
}

ExplorationList NodeIO::getNonSignalDriven(size_t outputPort) const
{
}
*/
void NodeIO::setOutputConnectionType(size_t outputPort, const ConnectionType &connectionType) 
{ 
    if (m_outputPorts[outputPort].connectionType != connectionType) {
        HCL_ASSERT_HINT(m_outputPorts[outputPort].connections.empty(), "The connection type of the output can not change once a node has connected to it!");        
        m_outputPorts[outputPort].connectionType = connectionType; 
    }
}

void NodeIO::setOutputType(size_t outputPort, OutputType outputType)
{
    m_outputPorts[outputPort].outputType = outputType;
}



void NodeIO::connectInput(size_t inputPort, const NodePort &output)
{
    auto &inPort = m_inputPorts[inputPort];
    if (inPort.node == output.node && inPort.port == output.port)
        return;
    
    if (inPort.node != nullptr)
        disconnectInput(inputPort);
    
    inPort = output;
    if (inPort.node != nullptr) {
        auto &outPort = inPort.node->m_outputPorts[inPort.port];
        outPort.connections.push_back({.node = static_cast<BaseNode*>(this), .port = inputPort});
    }
}

void NodeIO::disconnectInput(size_t inputPort)
{
    auto &inPort = m_inputPorts[inputPort];
    if (inPort.node != nullptr) {
        auto &outPort = inPort.node->m_outputPorts[inPort.port];
        
        auto it = std::find(
                        outPort.connections.begin(),
                        outPort.connections.end(),
                        NodePort{.node = static_cast<BaseNode*>(this), .port = inputPort});
        
        HCL_ASSERT(it != outPort.connections.end());
        
        std::swap(*it, outPort.connections.back());
        outPort.connections.pop_back();
        
        inPort.node = nullptr;
        inPort.port = INV_PORT;
    }
}

void NodeIO::resizeInputs(size_t num)
{
    if (num < m_inputPorts.size())
        for (auto i : utils::Range(num, m_inputPorts.size()))
            disconnectInput(i);
    m_inputPorts.resize(num);
}

void NodeIO::resizeOutputs(size_t num)
{
    if (num < m_outputPorts.size())
        for (auto i : utils::Range(num, m_outputPorts.size()))
            while (!m_outputPorts[i].connections.empty()) {
                auto &con = m_outputPorts[i].connections.front();
                con.node->disconnectInput(con.port);
            }
            
    m_outputPorts.resize(num);
}

        
}
