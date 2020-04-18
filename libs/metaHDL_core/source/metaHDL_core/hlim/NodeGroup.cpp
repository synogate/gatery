#include "NodeGroup.h"

#include "coreNodes/Node_Signal.h"

namespace mhdl {
namespace core {
namespace hlim {

NodeGroup *NodeGroup::addChildNodeGroup()
{
    m_children.push_back(std::make_unique<NodeGroup>());
    m_children.back()->m_parent = this;
    return m_children.back().get();
}



bool NodeGroup::isChildOf(const NodeGroup *other) const
{
    const NodeGroup *parent = getParent();
    while (parent != nullptr) {
        if (parent == other)
            return true;
        parent = parent->getParent();
    }
    return false;
}




void NodeGroup::cullUnnamedSignalNodes()
{
    for (auto &c : m_children)
        c->cullUnnamedSignalNodes();
    
    for (size_t i = 0; i < m_nodes.size(); i++) {
        Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
        if (signal == nullptr)
            continue;
            
        if (!signal->getName().empty())
            continue;
        
        if (signal->getInput(0).connection != nullptr &&
            signal->getInput(0).connection->node->getGroup() != this)
            continue;

        bool inputIsSignalOrUnconnected = (signal->getInput(0).connection == nullptr) || (dynamic_cast<Node_Signal*>(signal->getInput(0).connection->node) != nullptr);

        bool allFollowupsInGroup = true;
        bool allOutputsAreSignals = true;
        for (auto &c : signal->getOutput(0).connections) {
            allOutputsAreSignals &= (dynamic_cast<Node_Signal*>(c->node) != nullptr);
            
            if (c->node->getGroup() != this) {
                allFollowupsInGroup = false;
                break;
            }
        }
        
        if (!allFollowupsInGroup)
            continue;
        
        if (inputIsSignalOrUnconnected || allOutputsAreSignals) {
            
            Node::OutputPort *newSource = signal->getInput(0).connection;
            
            while (!signal->getOutput(0).connections.empty()) {
                Node::InputPort *port = *signal->getOutput(0).connections.begin();
                signal->getOutput(0).disconnect(*port);
                if (newSource != nullptr)
                    newSource->connect(*port);
            }
        
            m_nodes[i] = std::move(m_nodes.back());
            m_nodes.pop_back();
            i--;
        }
    }
}


}
}
}
