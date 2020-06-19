#include "Circuit.h"

#include "Node.h"
#include "NodeIO.h"
#include "coreNodes/Node_Signal.h"


namespace hcl::core::hlim {

Circuit::Circuit()
{
    m_root.reset(new NodeGroup(NodeGroup::GRP_ENTITY));
}

void Circuit::cullUnnamedSignalNodes()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
        if (signal == nullptr)
            continue;
            
        if (!signal->getName().empty())
            continue;
        
        bool inputIsSignalOrUnconnected = (signal->getDriver(0).node == nullptr) || (dynamic_cast<Node_Signal*>(signal->getDriver(0).node) != nullptr);

        bool allOutputsAreSignals = true;
        for (const auto &c : signal->getDirectlyDriven(0)) {
            allOutputsAreSignals &= (dynamic_cast<Node_Signal*>(c.node) != nullptr);
        }
        
        if (inputIsSignalOrUnconnected || allOutputsAreSignals) {
            
            NodePort newSource = signal->getDriver(0);
            
            while (!signal->getDirectlyDriven(0).empty()) {
                auto p = signal->getDirectlyDriven(0).front();
                p.node->connectInput(p.port, newSource);
            }
            
            signal->disconnectInput();
        
            m_nodes[i] = std::move(m_nodes.back());
            m_nodes.pop_back();
            i--;
        }
    }
}

void Circuit::cullOrphanedSignalNodes()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
        if (signal == nullptr)
            continue;
        
        if (signal->isOrphaned()) {
            m_nodes[i] = std::move(m_nodes.back());
            m_nodes.pop_back();
            i--;
        }
    }
}


}
