/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "gatery/pch.h"

#include "Subnet.h"

#include "supportNodes/Node_ExportOverride.h"
#include "supportNodes/Node_SignalTap.h"


#include "Circuit.h"
#include "Node.h"
#include "NodePort.h"
#include "NodeGroup.h"


namespace gtry::hlim {

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::all(CircuitType &circuit)
{
    FinalType res;
    res.addAll(circuit);
    return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allDrivenByOutputs(std::span<NodePort> outputs, std::span<NodePort> limitingInputs)
{
    FinalType res;
    res.addAllDrivenByOutputs(outputs, limitingInputs);
    return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allNecessaryForInputs(std::span<NodePort> limitingOutputs, std::span<NodePort> inputs)
{
    FinalType res;
    res.addAllNecessaryForInputs(limitingOutputs, inputs);
    return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allNecessaryForNodes(std::span<NodeType*> limitingNodes, std::span<NodeType*> nodes)
{
    FinalType res;
    res.addAllNecessaryForNodes(limitingNodes, nodes);
    return res;
}


template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allDrivenCombinatoricallyByOutputs(std::span<NodePort> outputs)
{
    FinalType res;
    res.addAllDrivenCombinatoricallyByOutputs(outputs);
    return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allForSimulation(CircuitType &circuit, const std::set<hlim::NodePort> &outputs)
{
    FinalType res;
    res.addAllForSimulation(circuit, outputs);
    return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allForExport(CircuitType &circuit, const utils::ConfigTree &exportSelectionConfig)
{
    FinalType res;
    res.addAllForExport(circuit, exportSelectionConfig);
    return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allUsedNodes(CircuitType &circuit)
{
    FinalType res;
    res.addAllUsedNodes(circuit);
    return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::fromNodeGroup(NodeGroup* nodeGroup, bool reccursive)
{
    FinalType res;
    res.addAllFromNodeGroup(nodeGroup, reccursive);
    return res;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::add(NodeType *node)
{
    m_nodes.insert(node);
    return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::remove(NodeType *node)
{
    m_nodes.erase(node);
    return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllDrivenByOutputs(std::span<NodePort> outputs, std::span<NodePort> limitingInputs)
{
    HCL_ASSERT_HINT(false, "Not yet implemented");
    return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllNecessaryForInputs(std::span<NodePort> limitingOutputs, std::span<NodePort> inputs)
{
    std::vector<NodeType*> openList;
    std::set<NodeType*> foundNodes;

    // Find roots
    for (auto &np : inputs) {
        auto driver = np.node->getDriver(np.port);
        if (driver.node != nullptr)
            openList.push_back(driver.node);
    }

    // Find limits
    for (auto &np : limitingOutputs)
        foundNodes.insert(np.node);

    // Find dependencies
    while (!openList.empty()) {
        auto *n = openList.back();
        openList.pop_back();

        if (foundNodes.contains(n)) continue; // already handled
        foundNodes.insert(n);
        m_nodes.insert(n);
        
        for (auto i : utils::Range(n->getNumInputPorts())) {
            auto driver = n->getDriver(i);
            if (driver.node != nullptr)
                openList.push_back(driver.node);
        }
    }

    return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllNecessaryForNodes(std::span<NodeType*> limitingNodes, std::span<NodeType*> nodes)
{
    std::vector<NodeType*> openList(nodes.begin(), nodes.end());
    std::set<NodeType*> foundNodes(limitingNodes.begin(), limitingNodes.end());

    // Find dependencies
    while (!openList.empty()) {
        auto *n = openList.back();
        openList.pop_back();

        if (foundNodes.contains(n)) continue; // already handled
        foundNodes.insert(n);
        m_nodes.insert(n);
        
        for (auto i : utils::Range(n->getNumInputPorts())) {
            auto driver = n->getDriver(i);
            if (driver.node != nullptr)
                openList.push_back(driver.node);
        }
    }

    return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllDrivenCombinatoricallyByOutputs(std::span<NodePort> outputs)
{
    std::vector<NodeType*> openList;

    for (auto &o : outputs)
        for (auto &c : o.node->getDirectlyDriven(o.port))
            openList.push_back(c.node);

    std::set<NodeType*> foundNodes;

    // Find dependencies
    while (!openList.empty()) {
        auto *n = openList.back();
        openList.pop_back();

        if (foundNodes.contains(n)) continue; // already handled
        foundNodes.insert(n);

        if (!n->isCombinatorial()) continue;
        m_nodes.insert(n);
        
        for (auto i : utils::Range(n->getNumOutputPorts())) 
            for (auto &c : n->getDirectlyDriven(i))
                openList.push_back(c.node);
    }

    return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAll(CircuitType &circuit)
{
    for (auto &n : circuit.getNodes())
        m_nodes.insert(n.get());
    return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllForSimulation(CircuitType &circuit, const std::set<hlim::NodePort> &outputs)
{
    std::vector<NodeType*> openList;
    std::set<NodeType*> handledNodes;

    // Find roots
    if (outputs.empty()) {
        for (auto &n : circuit.getNodes())
            if (n->hasSideEffects() || n->hasRef())
                openList.push_back(n.get());
    } else {
        for (auto np : outputs)
            openList.push_back(np.node);
    }

    // Find dependencies
    while (!openList.empty()) {
        auto *n = openList.back();
        openList.pop_back();

        if (handledNodes.contains(n)) continue; // already handled
        handledNodes.insert(n);

        // Ignore the export-only part as well as the export node
        if (dynamic_cast<ConstAdaptor<makeConst, hlim::Node_ExportOverride>::type*>(n)) {
            if (n->getDriver(0).node != nullptr)
                openList.push_back(n->getDriver(0).node);
        } else {
            m_nodes.insert(n);
            for (auto i : utils::Range(n->getNumInputPorts())) {
                auto driver = n->getDriver(i);
                if (driver.node != nullptr)
                    openList.push_back(driver.node);
            }
        }
    }

    return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllForExport(CircuitType &circuit, const utils::ConfigTree &exportSelectionConfig)
{
    bool includeAsserts = exportSelectionConfig["include_asserts"].as(false);

    std::vector<NodeType*> openList;
    std::set<NodeType*> handledNodes;

    // Find roots
    for (auto &n : circuit.getNodes())
        if (n->hasSideEffects()) {
            if (auto sigTap = dynamic_cast<Node_SignalTap*>(n.get())) {
                if (sigTap->getLevel() != Node_SignalTap::LVL_ASSERT && sigTap->getLevel() != Node_SignalTap::LVL_WARN) continue; // Only (potentially) want asserts in export.
                if (sigTap->getTrigger() != Node_SignalTap::TRIG_FIRST_INPUT_HIGH && sigTap->getTrigger() != Node_SignalTap::TRIG_FIRST_INPUT_LOW) continue; 
                if (!includeAsserts) continue; // ... and potentially not even those.
            }
            openList.push_back(n.get());
        }

    // Find dependencies
    while (!openList.empty()) {
        auto *n = openList.back();
        openList.pop_back();

        if (handledNodes.contains(n)) continue; // already handled
        handledNodes.insert(n);

        // Ignore the simulation-only part
        if (dynamic_cast<ConstAdaptor<makeConst, hlim::Node_ExportOverride>::type*>(n)) {
            if (n->getDriver(1).node != nullptr)
                openList.push_back(n->getDriver(1).node);
        } else {
            for (auto i : utils::Range(n->getNumInputPorts())) {
                auto driver = n->getDriver(i);
                if (driver.node != nullptr)
                    openList.push_back(driver.node);
            }
        }
    }
    m_nodes.insert(handledNodes.begin(), handledNodes.end());

    return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllUsedNodes(CircuitType &circuit)
{
    std::vector<NodeType*> openList;
    std::set<NodeType*> usedNodes;

    // Find roots
    for (auto &n : circuit.getNodes())
        if (n->hasSideEffects() || n->hasRef())
            openList.push_back(n.get());

    // Find dependencies
    while (!openList.empty()) {
        auto *n = openList.back();
        openList.pop_back();

        if (usedNodes.contains(n)) continue; // already handled
        usedNodes.insert(n);
        
        for (auto i : utils::Range(n->getNumInputPorts())) {
            auto driver = n->getDriver(i);
            if (driver.node != nullptr)
                openList.push_back(driver.node);
        }
    }

    m_nodes.insert(usedNodes.begin(), usedNodes.end());

    return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllFromNodeGroup(NodeGroup* nodeGroup, bool reccursive)
{
    m_nodes.insert(nodeGroup->getNodes().begin(), nodeGroup->getNodes().end());
    if (reccursive)
        for (auto& c : nodeGroup->getChildren())
            addAllFromNodeGroup(c.get(), reccursive);

    return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
void SubnetTemplate<makeConst, FinalType>::dilate(bool forward, bool backward)
{
    std::vector<NodeType*> newNodes;
    for (auto n : m_nodes) {
        if (backward)
            for (auto i : utils::Range(n->getNumInputPorts())) {
                auto np = n->getDriver(i);
                if (np.node != nullptr && !m_nodes.contains(np.node))
                    newNodes.push_back(np.node);
            }

        if (forward)
            for (auto i : utils::Range(n->getNumOutputPorts())) {
                for (auto np : n->getDirectlyDriven(i))
                    if (!m_nodes.contains(np.node))
                        newNodes.push_back(np.node);
            }
    }

    m_nodes.insert(newNodes.begin(), newNodes.end());
}



template class SubnetTemplate<false, Subnet>;
template class SubnetTemplate<true, ConstSubnet>;


}
