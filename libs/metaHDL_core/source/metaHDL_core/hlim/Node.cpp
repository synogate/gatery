#include "Node.h"

#include "NodeGroup.h"

#include "../utils/Exceptions.h"
#include "../utils/Range.h"

namespace mhdl {
namespace core {    
namespace hlim {


Node::Node(NodeGroup *group, unsigned numInputs, unsigned numOutputs) : m_nodeGroup(group)//, m_nodeGroupListEntry(*this)
{
    //m_nodeGroup->getNodes().insertBack(m_nodeGroupListEntry);
    m_inputs.resize(numInputs, InputPort(this));
    m_outputs.resize(numOutputs, OutputPort(this));
}


bool Node::isOrphaned() const
{
    for (const auto &port : m_inputs)
        if (port.connection != nullptr) return false;
    for (const auto &port : m_outputs)
        if (!port.connections.empty()) return false;
        
    return true;
}

Node::InputPort::~InputPort()
{
    if (connection != nullptr)
        connection->disconnect(*this);
}

Node::OutputPort::~OutputPort()
{
    while (!connections.empty())
        disconnect(**connections.begin());
}

void Node::OutputPort::connect(InputPort &port)
{
    MHDL_ASSERT_HINT(port.connection == nullptr, "Can only connect previously unconnected ports!");
    port.connection = this;
    connections.insert(&port);
}

void Node::OutputPort::disconnect(InputPort &port)
{
    MHDL_ASSERT_HINT(port.connection == this, "InputPort not connected to the OutputPort it is to be disconnected from!");
    port.connection = nullptr;
    
    auto it = connections.find(&port);
    MHDL_ASSERT_HINT(it != connections.end(), "InputPort not in list of connections!");
    connections.erase(it);
}

void Node::moveToGroup(NodeGroup *group)
{
    if (group == m_nodeGroup) return;
    
    unsigned index = ~0u;
    for (auto i : utils::Range(m_nodeGroup->m_nodes.size()))
        if (m_nodeGroup->m_nodes[i].get() == this) {
            index = i;
            break;
        }
    MHDL_ASSERT(index != ~0u);

    group->m_nodes.push_back(std::move(m_nodeGroup->m_nodes[index]));

    m_nodeGroup->m_nodes[index] = std::move(m_nodeGroup->m_nodes.back());
    m_nodeGroup->m_nodes.pop_back();
    
    m_nodeGroup = group;
}


}
}
}
