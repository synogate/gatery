#pragma once

#include "NodeGroup.h"
#include "Node.h"
#include "ConnectionType.h"
#include "Clock.h"

#include <vector>
#include <memory>

namespace mhdl::core::hlim {

class Circuit
{
    public:
        Circuit();
        
        template<typename NodeType, typename... Args>
        NodeType *createNode(Args&&... args);        
        
        template<typename ClockType, typename... Args>
        ClockType *createClock(Args&&... args);        
        
        inline NodeGroup *getRootNodeGroup() { return m_root.get(); }
        inline const NodeGroup *getRootNodeGroup() const { return m_root.get(); }

        inline const std::vector<std::unique_ptr<BaseNode>> &getNodes() const { return m_nodes; }
        inline const std::vector<std::unique_ptr<BaseClock>> &getClocks() const { return m_clocks; }

        void cullUnnamedSignalNodes();
        void cullOrphanedSignalNodes();
    protected:
        std::vector<std::unique_ptr<BaseNode>> m_nodes;
        std::unique_ptr<NodeGroup> m_root;
        std::vector<std::unique_ptr<BaseClock>> m_clocks;
};


template<typename NodeType, typename... Args>
NodeType *Circuit::createNode(Args&&... args) {
    m_nodes.push_back(std::make_unique<NodeType>(std::forward<Args>(args)...));
    return (NodeType *) m_nodes.back().get();
}

template<typename ClockType, typename... Args>
ClockType *Circuit::createClock(Args&&... args) {
    m_clocks.push_back(std::make_unique<ClockType>(std::forward<Args>(args)...));
    return (ClockType *) m_clocks.back().get();
}

}
