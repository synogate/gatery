#include "Circuit.h"

#include "Node.h"
#include "NodeIO.h"
#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Multiplexer.h"
#include "coreNodes/Node_Logic.h"


#include <set>
#include <vector>
#include <map>

#include <iostream>

namespace hcl::core::hlim {

Circuit::Circuit()
{
    m_root.reset(new NodeGroup(NodeGroup::GroupType::ENTITY));
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

void Circuit::cullUnusedNodes()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        if (m_nodes[i]->hasSideEffects()) 
            continue;

        if (Node_Signal *signalNode = dynamic_cast<Node_Signal*>(m_nodes[i].get()))
            if (!signalNode->getName().empty())
                continue;

        bool isInUse = false;
        for (auto j : utils::Range(m_nodes[i]->getNumOutputPorts())) 
            if (!m_nodes[i]->getDirectlyDriven(j).empty()) {
                isInUse = true;
                break;
            }
            
        if (!isInUse) {
            std::cout << "Culling node " << m_nodes[i]->getTypeName() << " "  << m_nodes[i]->getName() << std::endl;
                
            m_nodes[i] = std::move(m_nodes.back());
            m_nodes.pop_back();
            i--;
        }
    }    
}


struct HierarchyCondition {
    std::map<NodePort, bool> m_conditionsAndNegations;
    bool m_undefined = false;
    bool m_contradicting = false;

    void parse(NodePort nodeInput) {
        std::vector<std::pair<NodePort, bool>> stack;
        if (nodeInput.node != nullptr)
            stack.push_back({nodeInput.node->getNonSignalDriver(nodeInput.port), false});
        else
            m_undefined = true;
        
        while (!stack.empty()) {
            auto top = stack.back();
            stack.pop_back();
            if (top.first.node == nullptr)
                m_undefined = true;
            else {
                if (Node_Logic *logicNode = dynamic_cast<Node_Logic*>(top.first.node)) {
                    if (logicNode->getOp() == Node_Logic::NOT) {
                        auto driver = logicNode->getNonSignalDriver(0);
                        stack.push_back({logicNode->getNonSignalDriver(0), !top.second});
                    } else 
                    if (logicNode->getOp() == Node_Logic::AND) {
                        for (auto j : utils::Range(logicNode->getNumInputPorts()))
                            stack.push_back({logicNode->getNonSignalDriver(j), top.second});
                    } else {
                        auto it = m_conditionsAndNegations.find(top.first);
                        if (it != m_conditionsAndNegations.end())
                            m_contradicting |= it->second != top.second;
                        else
                            m_conditionsAndNegations[top.first] = top.second;
                    }
                } else {
                    auto it = m_conditionsAndNegations.find(top.first);
                    if (it != m_conditionsAndNegations.end())
                        m_contradicting |= it->second != top.second;
                    else
                        m_conditionsAndNegations[top.first] = top.second;
                }
            }
        }
    }
    
    bool isEqualOf(const HierarchyCondition &other) const {
        if (m_undefined || other.m_undefined) return false;
        if (m_contradicting && other.m_contradicting) return true;
        
        if (m_conditionsAndNegations.size() != other.m_conditionsAndNegations.size()) return false;
        for (const auto &pair : m_conditionsAndNegations) {
            auto it = other.m_conditionsAndNegations.find(pair.first);
            if (it == other.m_conditionsAndNegations.end()) return false;
            if (it->second != pair.second) return false;
        }
        return true;
    }
    
    bool isNegationOf(const HierarchyCondition &other) const {
        if (m_undefined || other.m_undefined) return false;
        if (m_contradicting && other.m_contradicting) return false;
        
        if (m_conditionsAndNegations.size() != other.m_conditionsAndNegations.size()) return false;
        for (const auto &pair : m_conditionsAndNegations) {
            auto it = other.m_conditionsAndNegations.find(pair.first);
            if (it == other.m_conditionsAndNegations.end()) return false;
            if (it->second == pair.second) return false;
        }
        return true;
    }

    bool isSubsetOf(const HierarchyCondition &other) const {
        if (m_undefined || other.m_undefined) return false;
        if (m_contradicting && other.m_contradicting) return false;
        
        for (const auto &pair : m_conditionsAndNegations) {
            auto it = other.m_conditionsAndNegations.find(pair.first);
            if (it == other.m_conditionsAndNegations.end()) return false;
            if (it->second != pair.second) return false;
        }
        return true;
    }
};

void Circuit::mergeMuxes()
{
    bool done;
    do {
        done = true;
        
        for (size_t i = 0; i < m_nodes.size(); i++) {
            if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(m_nodes[i].get())) {
                if (muxNode->getNumInputPorts() != 3) continue;

                std::cout << "Found 2-input mux" << std::endl;

                HierarchyCondition condition;
                condition.parse({.node = muxNode, .port = 0});
                
                for (size_t muxInput : utils::Range(2)) {
                
                    auto input0 = muxNode->getNonSignalDriver(muxInput?2:1);
                    auto input1 = muxNode->getNonSignalDriver(muxInput?1:2);
                    
                    if (input1.node == nullptr)
                        continue;
                    
                    if (Node_Multiplexer *prevMuxNode = dynamic_cast<Node_Multiplexer*>(input0.node)) {
                        std::cout << "Found 2 chained muxes" << std::endl;
                        
                        HierarchyCondition prevCondition;
                        prevCondition.parse({.node = prevMuxNode, .port = 0});                
                        
                        bool conditionsMatch = false;
                        bool prevConditionNegated;
                        
                        if (prevCondition.isEqualOf(condition)) {
                            conditionsMatch = true;
                            prevConditionNegated = muxInput==1;
                        } else if (condition.isNegationOf(prevCondition)) {
                            conditionsMatch = true;
                            prevConditionNegated = muxInput==0;
                        } else {
                            std::cout << "Condition 1 is :" << std::endl;
                            if (condition.m_undefined) std::cout << "   undefined" << std::endl;
                            if (condition.m_contradicting) std::cout << "   contradicting" << std::endl;
                            std::cout << "    ";
                            for (auto p : condition.m_conditionsAndNegations) {
                                std::cout << " and ";
                                if (p.second)
                                    std::cout << "not ";
                                std::cout << std::hex << p.first.node << ':' << p.first.port;
                            }
                            std::cout << std::endl;
                            std::cout << "Condition 2 is :" << std::endl;
                            if (prevCondition.m_undefined) std::cout << "   undefined" << std::endl;
                            if (prevCondition.m_contradicting) std::cout << "   contradicting" << std::endl;
                            std::cout << "    ";
                            for (auto p : prevCondition.m_conditionsAndNegations) {
                                std::cout << " and ";
                                if (p.second)
                                    std::cout << "not ";
                                std::cout << std::hex << p.first.node << ':' << p.first.port;
                            }
                            std::cout << std::endl;
                        }
                        
                        if (conditionsMatch) {
                            std::cout << "Conditions match!" << std::endl;
                            
                            auto bypass = prevMuxNode->getDriver(prevConditionNegated?2:1);
                            auto prevOp = prevMuxNode->getDriver(prevConditionNegated?1:2);
                            
                            done = false;
                            
                            // Connect second mux directly to bypass
                            muxNode->connectInput(muxInput, bypass);
#if 0                            
                            // Connect second logic op block directly to output of first logic op block
                            for (auto prevMuxOutput : prevMuxNode->getDirectlyDriven(0)) {
                                std::vector<BaseNode*> openList = { prevMuxOutput.node };
                                std::set<BaseNode*> closedList;

                                bool allSubnetOutputsMuxed = true;
                                
                                while (!openList.empty()) {
                                    BaseNode *node = openList.back();
                                    openList.pop_back();
                                    if (closedList.contains(node)) continue;
                                    closedList.insert(node);
                                    
                                    if (node->hasSideEffects()) {
                                        allSubnetOutputsMuxed = false;
                                        std::cout << "Internal node with sideeffects, skipping" << std::endl;
                                        break;
                                    }
                                    
                                    if (node->getGroup() != muxNode->getGroup()) {
                                        allSubnetOutputsMuxed = false;
                                        std::cout << "Internal node driving external, skipping" << std::endl;
                                        break;
                                    }
                                    
                                    if (Node_Multiplexer *subnetOutputMuxNode = dynamic_cast<Node_Multiplexer*>(node)) {
                                        HierarchyCondition subnetOutputMuxNodeCondition;
                                        subnetOutputMuxNodeCondition.parse({.node = subnetOutputMuxNode, .port = 0});                
                                        
                                        if (prevCondition.isEqualOf(subnetOutputMuxNodeCondition)) {
                                            continue;
                                        }
                                    }
                                    
                                    for (auto j : utils::Range(node->getNumOutputPorts()))
                                        for (auto driven : node->getDirectlyDriven(j)) 
                                            openList.push_back(driven.node);
                                        
                                }
                                
                                if (allSubnetOutputsMuxed) {
                                    std::cout << "Rewiring past mux" << std::endl;
                                    prevMuxOutput.node->connectInput(prevMuxOutput.port, prevOp);
                                } else {
                                    std::cout << "Not rewiring past mux" << std::endl;
                                }
                            }
#endif
                        }
                    }
                }
            }
        }
    } while (!done);
}


void Circuit::removeIrrelevantMuxes()
{
    bool done;
    do {
        done = true;
        
        for (size_t i = 0; i < m_nodes.size(); i++) {
            if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(m_nodes[i].get())) {
                if (muxNode->getNumInputPorts() != 3) continue;

                std::cout << "Found 2-input mux" << std::endl;

                HierarchyCondition condition;
                condition.parse({.node = muxNode, .port = 0});
                
                for (size_t muxInputPort : utils::Range(1,3)) {
                
                    for (auto muxOutput : muxNode->getDirectlyDriven(0)) {
                        std::vector<NodePort> openList = { muxOutput };
                        std::set<NodePort> closedList;

                        bool allSubnetOutputsMuxed = true;
                        
                        while (!openList.empty()) {
                            NodePort input = openList.back();
                            openList.pop_back();
                            if (closedList.contains(input)) continue;
                            closedList.insert(input);
                            
                            if (input.node->hasSideEffects()) {
                                allSubnetOutputsMuxed = false;
                                std::cout << "Internal node with sideeffects, skipping" << std::endl;
                                break;
                            }
                            
                            if (input.node->getGroup() != muxNode->getGroup()) {
                                allSubnetOutputsMuxed = false;
                                std::cout << "Internal node driving external, skipping" << std::endl;
                                break;
                            }
                            
                            if (Node_Multiplexer *subnetOutputMuxNode = dynamic_cast<Node_Multiplexer*>(input.node)) {
                                if (muxNode->getNumInputPorts() == 3) {
                                    HierarchyCondition subnetOutputMuxNodeCondition;
                                    subnetOutputMuxNodeCondition.parse({.node = subnetOutputMuxNode, .port = 0});                
                                    
                                    if (input.port == muxInputPort && condition.isEqualOf(subnetOutputMuxNodeCondition))
                                        continue;
                                    if (input.port != muxInputPort && condition.isNegationOf(subnetOutputMuxNodeCondition))
                                        continue;
                                }
                            }
                            
                            for (auto j : utils::Range(input.node->getNumOutputPorts()))
                                for (auto driven : input.node->getDirectlyDriven(j)) 
                                    openList.push_back(driven);
                                
                        }
                        
                        if (allSubnetOutputsMuxed) {
                            std::cout << "Rewiring past mux" << std::endl;
                            muxOutput.node->connectInput(muxOutput.port, muxNode->getDriver(muxInputPort));
                            done = false;
                        } else {
                            std::cout << "Not rewiring past mux" << std::endl;
                        }
                    }
                }
            }
        }
    } while (!done);
}


void Circuit::cullMuxConditionNegations()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(m_nodes[i].get())) {
            if (muxNode->getNumInputPorts() != 3) continue;

            auto condition = muxNode->getNonSignalDriver(0);
            
            if (Node_Logic *logicNode = dynamic_cast<Node_Logic*>(condition.node)) {
                if (logicNode->getOp() == Node_Logic::NOT) {
                    muxNode->connectSelector(logicNode->getDriver(0));
                    
                    auto input0 = muxNode->getDriver(1);
                    auto input1 = muxNode->getDriver(2);
                    
                    muxNode->connectInput(0, input1);
                    muxNode->connectInput(1, input0);
                    
                    i--; // check same mux again to unravel chain of nots
                }
            }
        }
    }
}


void Circuit::optimize(size_t level)
{
    switch (level) {
        case 0:
        break;
        case 1:
            cullOrphanedSignalNodes();
        break;
        case 2:
            cullOrphanedSignalNodes();
            cullUnnamedSignalNodes();
            cullUnusedNodes();
        break;
        case 3:
            cullOrphanedSignalNodes();
            cullUnnamedSignalNodes();
            mergeMuxes();
            removeIrrelevantMuxes();
            cullMuxConditionNegations();
            cullUnusedNodes();
        break;
    };
}

}
