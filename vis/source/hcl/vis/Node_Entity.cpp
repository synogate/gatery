#include "Node_Entity.h"

#include "CircuitView.h"

#include <hcl/utils/Range.h>

#include <set>

namespace hcl::vis {

Node_Entity::Node_Entity(CircuitView *circuitView, core::hlim::NodeGroup *nodeGroup) : Node(circuitView), m_hlimNodeGroup(nodeGroup)
{
    
    std::set<core::hlim::NodePort> inputsFound;
    std::set<core::hlim::NodePort> outputsFound;
    
    std::vector<core::hlim::NodeGroup*> groupStack = { m_hlimNodeGroup };
    
    while (!groupStack.empty()) {
        core::hlim::NodeGroup *nodeGroup = groupStack.back();
        groupStack.pop_back();
        
        for (auto &subGroup : nodeGroup->getChildren()) 
            groupStack.push_back(subGroup.get());
    
        // Find all explicit signals/variables (signals that will need to be declared and assigned)
        for (auto node : nodeGroup->getNodes()) {
            // Check for inputs
            for (auto i : utils::Range(node->getNumInputPorts())) {
                auto driver = node->getDriver(i);

                if (driver.node == nullptr) {
//                    std::cout << "Warning: Unconnected node: Port " << i << " of node '"<<node->getName()<<"' not connected!" << std::endl;
//                    std::c#include "CircuitView.h"out << node->getStackTrace() << std::endl;
                } else if (driver.node->getGroup() == nullptr || (driver.node->getGroup() != m_hlimNodeGroup && !driver.node->getGroup()->isChildOf(m_hlimNodeGroup))) {
                    inputsFound.insert(driver);
                }
            }
            
            // Check for outputs
            for (auto i : utils::Range(node->getNumOutputPorts())) {
                if (node->getDirectlyDriven(i).empty()) {
//                    std::cout << "Warning: Unused node: Port " << i << " of node '"<<node->getName()<<"' not connected!" << std::endl;
//                    std::cout << node->getStackTrace() << std::endl;
                }
                core::hlim::NodePort driver;
                driver.node = node;
                driver.port = i;
                
                for (auto driven : node->getDirectlyDriven(i)) {
                    if (driven.node->getGroup() == nullptr || (driven.node->getGroup() != m_hlimNodeGroup && !driven.node->getGroup()->isChildOf(m_hlimNodeGroup))) {
                        outputsFound.insert(driver);
                    }
                }
            }
        }
    }
    
    for (auto p : inputsFound) {
        m_inputPorts.push_back({
            .name = p.node->getName(),
            .graphicsItem = nullptr,
            .producer = p
        });
    }
    for (auto p : outputsFound) {
        m_outputPorts.push_back({
            .name = p.node->getName(),
            .graphicsItem = nullptr,
            .producer = p
        });
    }
    
    m_name = m_hlimNodeGroup->getName();
    
    createDefaultGraphics(200);

    dynamic_cast<QGraphicsRectItem*>(m_background)->setBrush(QBrush(QColor(128, 128, 128)));
}

}
