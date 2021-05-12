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
#include "Circuit.h"

#include "Node.h"
#include "NodeIO.h"
#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Multiplexer.h"
#include "coreNodes/Node_Logic.h"
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Pin.h"
#include "coreNodes/Node_Rewire.h"
#include "coreNodes/Node_Register.h"

#include "MemoryDetector.h"


#include "../simulation/BitVectorState.h"
#include "../utils/Range.h"


#include <set>
#include <vector>
#include <map>

#include <iostream>

namespace gtry::hlim {

Circuit::Circuit()
{
    m_root.reset(new NodeGroup(NodeGroup::GroupType::ENTITY));
}

/**
 * @brief Copies a subnet of all nodes that are needed to drive the specified outputs up to the specified inputs
 * @note Do note that subnetInputs specified input ports, not output ports!
 * @param source The circuit to copy from
 * @param subnetInputs Input ports where to stop copying
 * @param subnetOutputs Output ports from where to start copying.
 * @param mapSrc2Dst Map from all copied nodes in the source circuit to the corresponding node in the destination circuit.
 */
void Circuit::copySubnet(const std::vector<NodePort> &subnetInputs,
                         const std::vector<NodePort> &subnetOutputs,
                         std::map<BaseNode*, BaseNode*> &mapSrc2Dst)
{
    mapSrc2Dst.clear();

    std::set<NodePort> subnetInputSet(subnetInputs.begin(), subnetInputs.end());

    std::set<BaseNode*> closedList;
    std::vector<NodePort> openList = subnetOutputs;

    std::vector<std::pair<std::uint64_t, BaseNode*>> sortedNodes;

    // Scan and copy unconnected nodes
    while (!openList.empty()) {
        NodePort nodePort = openList.back();
        openList.pop_back();
        if (closedList.contains(nodePort.node)) continue;
        closedList.insert(nodePort.node);

        BaseNode *newNode = createUnconnectedClone(nodePort.node);
        mapSrc2Dst[nodePort.node] = newNode;
        sortedNodes.push_back({nodePort.node->getId(), newNode});
        for (auto i : utils::Range(nodePort.node->getNumInputPorts())) {
            auto driver = nodePort.node->getDriver(i);
            if (driver.node == nullptr) continue;
            if (subnetInputSet.contains({.node=nodePort.node, .port=i})) continue;
            openList.push_back(driver);
        }
    }

    // Sort new nodes based on old ids to give them new ids (unique to this circuit) while preserving the order
    std::sort(sortedNodes.begin(), sortedNodes.end());
    for (auto [oldId, node] : sortedNodes)
        node->setId(m_nextNodeId++, {});

    // Reestablish connections, create clock network on demand
    std::map<Clock*, Clock*> mapSrc2Dst_clocks;
    std::function<Clock*(Clock*)> lazyCreateClockNetwork;

    lazyCreateClockNetwork = [&](Clock *oldClock)->Clock*{
        auto it = mapSrc2Dst_clocks.find(oldClock);
        if (it != mapSrc2Dst_clocks.end()) return it->second;

        Clock *oldParent = oldClock->getParentClock();
        Clock *newParent = nullptr;
        if (oldParent != nullptr)
            newParent = lazyCreateClockNetwork(oldParent);
        return createUnconnectedClock(oldClock, newParent);
    };


    for (auto oldNew : mapSrc2Dst) {
        auto oldNode = oldNew.first;
        auto newNode = oldNew.second;
        for (auto i : utils::Range(oldNode->getNumInputPorts())) {
            auto driver = oldNode->getDriver(i);
            if (driver.node == nullptr) continue;
            auto it = mapSrc2Dst.find(driver.node);
            if (it != mapSrc2Dst.end())
                newNode->rewireInput(i, {.node = it->second, .port=driver.port});
        }

        for (auto i : utils::Range(oldNode->getClocks().size())) {
            auto *oldClock = oldNode->getClocks()[i];
            if (oldClock != nullptr)
                newNode->attachClock(lazyCreateClockNetwork(oldClock), i);
        }
    }
}


BaseNode *Circuit::createUnconnectedClone(BaseNode *srcNode, bool noId)
{
    m_nodes.push_back(srcNode->cloneUnconnected());
    if (!noId)
        m_nodes.back()->setId(m_nextNodeId++, {});
    return m_nodes.back().get();
}


Clock *Circuit::createUnconnectedClock(Clock *clock, Clock *newParent)
{
    m_clocks.push_back(clock->cloneUnconnected(newParent));
    return m_clocks.back().get();
}

/**
 * @brief Infers signal names for all unnamed signals
 *
 */
void Circuit::inferSignalNames()
{
    std::set<Node_Signal*> unnamedSignals;
    for (size_t i = 0; i < m_nodes.size(); i++)
        if (auto *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get()))
            if (signal->getName().empty())
                unnamedSignals.insert(signal);


    while (!unnamedSignals.empty()) {

        Node_Signal *s = *unnamedSignals.begin();
        std::vector<Node_Signal*> signalsToName;
        std::set<Node_Signal*> loopDetection;

        signalsToName.push_back(s);
        loopDetection.insert(s);

        for (auto nh : s->exploreInput(0).skipDependencies()) {
            if (nh.isSignal()) {
                if (loopDetection.contains((Node_Signal*)nh.node())) {
                    nh.node()->setInferredName("loop");
                    nh.backtrack();
                } else
                    if (!nh.node()->getName().empty()) {
                        nh.backtrack();
                    } else {
                        signalsToName.push_back((Node_Signal*)nh.node());
                        loopDetection.insert((Node_Signal*)nh.node());
                    }
            }
        }

        for (auto it = signalsToName.rbegin(); it != signalsToName.rend(); ++it) {
            if ((*it)->getName().empty()) {
                auto driver = (*it)->getDriver(0);
                if (driver.node != nullptr) {
                    auto name = driver.node->attemptInferOutputName(driver.port);
                    if (!name.empty())
                        (*it)->setInferredName(std::move(name));
                } else
                    (*it)->setInferredName("undefined");
            }
            unnamedSignals.erase(*it);
        }
    }
}


/**
 * @brief Removes unnecessary and unnamed signal nodes
 *
 */
void Circuit::cullUnnamedSignalNodes()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
        if (signal == nullptr)
            continue;

        // Don't cull named signals
        if (!signal->getName().empty())
            continue;

        // Don't cull signals that are still referenced by frontend objects
        if (signal->hasRef()) continue;

        // We want to keep one signal node between non-signal nodes, so only cull if the input or all outputs are signals.
        bool inputIsSignalOrUnconnected = (signal->getDriver(0).node == nullptr) || (dynamic_cast<Node_Signal*>(signal->getDriver(0).node) != nullptr);

        bool allOutputsAreSignals = true;
        for (const auto &c : signal->getDirectlyDriven(0)) {
            allOutputsAreSignals &= (dynamic_cast<Node_Signal*>(c.node) != nullptr);
        }

        if (inputIsSignalOrUnconnected || allOutputsAreSignals) {

            if (signal->getDriver(0).node != signal)
                signal->bypassOutputToInput(0, 0);
            signal->disconnectInput();

            m_nodes[i] = std::move(m_nodes.back());
            m_nodes.pop_back();
            i--;
        }
    }
}

/**
 * @brief Removes signal nodes that are unnecessary because there is a sequence of identical ones
 *
 */
void Circuit::cullSequentiallyDuplicatedSignalNodes()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
        if (signal == nullptr)
            continue;

        // Don't cull signals that are still referenced by frontend objects
        if (signal->hasRef()) continue;

        auto driver = signal->getDriver(0);

        if (auto *driverSignal = dynamic_cast<Node_Signal*>(driver.node)) {

            if (driverSignal->getName() == signal->getName() &&
                driverSignal->getComment() == signal->getComment() &&
                driverSignal->getGroup() == signal->getGroup() &&
                driverSignal->getSignalGroup() == signal->getSignalGroup()) {

                if (driverSignal == signal) continue;
                
                signal->bypassOutputToInput(0, 0);
                signal->disconnectInput();

                m_nodes[i] = std::move(m_nodes.back());
                m_nodes.pop_back();
                i--;
            }
        }
    }
}

/**
 * @brief Cull signal nodes without inputs or outputs
 *
 */
void Circuit::cullOrphanedSignalNodes()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
        if (signal == nullptr)
            continue;

        // Don't cull signals that are still referenced by frontend objects
        if (signal->hasRef()) continue;

        if (signal->isOrphaned()) {
            m_nodes[i] = std::move(m_nodes.back());
            m_nodes.pop_back();
            i--;
        }
    }
}

static bool isUnusedNode(const BaseNode& node)
{
    if (node.hasSideEffects())
        return false;

    if (node.hasRef()) return false;
/*
    auto* signalNode = dynamic_cast<const Node_Signal*>(&node);
    if (signalNode && !signalNode->getName().empty())
        return false;
*/

    for (auto j : utils::Range(node.getNumOutputPorts()))
        if (!node.getDirectlyDriven(j).empty())
            return false;

    return true;
}

void Circuit::cullUnusedNodes()
{
    //std::cout << "cullUnusedNodes()" << std::endl;
    bool done;
    do {
        done = true;

        for (size_t i = 0; i < m_nodes.size(); i++) {

            if (isUnusedNode(*m_nodes[i]))
            {
                //std::cout << "    Culling node " << m_nodes[i]->getTypeName() << " "  << m_nodes[i]->getName() << std::endl;

                m_nodes[i] = std::move(m_nodes.back());
                m_nodes.pop_back();
                i--;
                done = false;
            }
        }
    } while (!done);
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
                        stack.push_back({driver, !top.second});
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

                //std::cout << "Found 2-input mux" << std::endl;

                HierarchyCondition condition;
                condition.parse({.node = muxNode, .port = 0});

                for (size_t muxInput : utils::Range(2)) {

                    auto input0 = muxNode->getNonSignalDriver(muxInput?2:1);
                    auto input1 = muxNode->getNonSignalDriver(muxInput?1:2);

                    if (input1.node == nullptr)
                        continue;

                    if (Node_Multiplexer *prevMuxNode = dynamic_cast<Node_Multiplexer*>(input0.node)) {
                        if (prevMuxNode == muxNode) continue; // sad thing

                        //std::cout << "Found 2 chained muxes" << std::endl;

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
                            /*
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
                            */
                        }

                        if (conditionsMatch) {
                            //std::cout << "Conditions match!" << std::endl;

                            auto bypass = prevMuxNode->getDriver(prevConditionNegated?2:1);
                            // Connect second mux directly to bypass
                            muxNode->connectInput(muxInput, bypass);

                            done = false;
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

                //std::cout << "Found 2-input mux" << std::endl;

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

                            if (input.node->hasSideEffects() || !input.node->isCombinatorial()) {
                                allSubnetOutputsMuxed = false;
                                //std::cout << "Internal node with sideeffects, skipping" << std::endl;
                                break;
                            }

                            if (input.node->getGroup() != muxNode->getGroup()) {
                                allSubnetOutputsMuxed = false;
                                //std::cout << "Internal node driving external, skipping" << std::endl;
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
                            //std::cout << "Rewiring past mux" << std::endl;
                            muxOutput.node->connectInput(muxOutput.port, muxNode->getDriver(muxInputPort));
                            done = false;
                        } else {
                            //std::cout << "Not rewiring past mux" << std::endl;
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

/**
 * @details So far only removes no-op rewire nodes since they prevent block-ram detection
 *
 */
void Circuit::removeNoOps()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        bool removeNode = false;

        if (auto *rewire = dynamic_cast<Node_Rewire*>(m_nodes[i].get())) {
            if (rewire->isNoOp()) {
                rewire->bypassOutputToInput(0, 0);
                removeNode = true;
            }
        }

        if (removeNode && !m_nodes[i]->hasRef()) {
            m_nodes[i] = std::move(m_nodes.back());
            m_nodes.pop_back();
            i--;
        }
    }
}

void Circuit::foldRegisterMuxEnableLoops()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        if (auto *regNode = dynamic_cast<Node_Register*>(m_nodes[i].get())) {
            auto enableCondition = regNode->getNonSignalDriver((unsigned)Node_Register::Input::ENABLE);

            auto data = regNode->getNonSignalDriver((unsigned)Node_Register::Input::DATA);
            if (auto *muxNode = dynamic_cast<Node_Multiplexer*>(data.node)) {
                if (muxNode->getNumInputPorts() == 3) {

                    auto muxInput1 = muxNode->getNonSignalDriver(1);
                    auto muxInput2 = muxNode->getNonSignalDriver(2);

                    auto muxCondition = muxNode->getDriver(0);

                    if (muxInput1.node == regNode) {
                        if (enableCondition.node != nullptr) {
                            auto *andNode = createNode<Node_Logic>(Node_Logic::AND);
                            andNode->recordStackTrace();
                            andNode->moveToGroup(regNode->getGroup());
                            andNode->connectInput(0, enableCondition);
                            andNode->connectInput(1, muxCondition);

                            regNode->connectInput(Node_Register::Input::ENABLE, {.node=andNode, .port=0ull});
                        } else {
                            regNode->connectInput(Node_Register::Input::ENABLE, muxCondition);
                        }
                        regNode->connectInput(Node_Register::Input::DATA, muxNode->getDriver(2));
                    } else if (muxInput2.node == regNode) {
                        auto *notNode = createNode<Node_Logic>(Node_Logic::NOT);
                        notNode->recordStackTrace();
                        notNode->moveToGroup(regNode->getGroup());
                        notNode->connectInput(0, muxCondition);

                        if (enableCondition.node != nullptr) {
                            auto *andNode = createNode<Node_Logic>(Node_Logic::AND);
                            andNode->recordStackTrace();
                            andNode->moveToGroup(regNode->getGroup());
                            andNode->connectInput(0, enableCondition);
                            andNode->connectInput(1, {.node=notNode,.port=0ull});

                            regNode->connectInput(Node_Register::Input::ENABLE, {.node=andNode, .port=0ull});
                        } else {
                            regNode->connectInput(Node_Register::Input::ENABLE, {.node=notNode, .port=0ull});
                        }
                        regNode->connectInput(Node_Register::Input::DATA, muxNode->getDriver(1));
                    }
                }
            }
        }
    }
}

void Circuit::removeConstSelectMuxes()
{
    for (size_t i = 0; i < m_nodes.size(); i++) {
        if (auto *muxNode = dynamic_cast<Node_Multiplexer*>(m_nodes[i].get())) {
            auto sel = muxNode->getNonSignalDriver(0);
            if (auto *constNode = dynamic_cast<Node_Constant*>(sel.node)) {
                HCL_ASSERT(constNode->getValue().size() < 64);
                std::uint64_t selDefined = constNode->getValue().extractNonStraddling(sim::DefaultConfig::DEFINED, 0, constNode->getValue().size());
                std::uint64_t selValue = constNode->getValue().extractNonStraddling(sim::DefaultConfig::VALUE, 0, constNode->getValue().size());
                if ((selDefined ^ (~0ull >> (64 - constNode->getValue().size()))) == 0) {
                    muxNode->bypassOutputToInput(0, 1+selValue);
                }
            }
        }
    }
}

void Circuit::propagateConstants()
{
    //std::cout << "propagateConstants()" << std::endl;
    sim::SimulatorCallbacks ignoreCallbacks;

    std::vector<NodePort> openList;
    // std::set<NodePort> closedList;

    // Start walking the graph from the const nodes
    for (size_t i = 0; i < m_nodes.size(); i++) {
        if (Node_Constant *constNode = dynamic_cast<Node_Constant*>(m_nodes[i].get())) {
            openList.push_back({.node = constNode, .port = 0});
        }
    }

    while (!openList.empty()) {
        NodePort constPort = openList.back();
        openList.pop_back();
        /*
        if (closedList.contains(constPort)) continue;
        closedList.insert(constPort);
        */

        // The output constPort is constant, loop over all nodes driven by this and look for nodes that can be computed
        for (auto successor : constPort.node->getDirectlyDriven(constPort.port)) {

            // Signal nodes don't do anything, so add the output of the signal node to the openList
            if (Node_Signal *signalNode = dynamic_cast<Node_Signal*>(successor.node)) {
                openList.push_back({.node = signalNode, .port = 0});
                continue;
            }

            // Remove registers without reset on the path
            if (auto *regNode = dynamic_cast<Node_Register*>(successor.node))
                if (regNode->getNonSignalDriver((unsigned)Node_Register::Input::RESET_VALUE).node == nullptr) {

                    regNode->bypassOutputToInput(0, (unsigned)Node_Register::Input::DATA);
                    regNode->disconnectInput(Node_Register::Input::DATA);
                    openList.push_back(constPort);
                    continue;
                }

            // Only work on combinatory nodes
            if (!successor.node->isCombinatorial()) continue;
            // Nodes with side-effects can't be removed/bypassed
            if (successor.node->hasSideEffects()) continue;

            if (!successor.node->getInternalStateSizes().empty()) continue; // can't be good for const propagation

            // Attempt to compute the output of this node.
            // Build a state vector with all inputs. Set all non-const inputs to undefined.
            sim::DefaultBitVectorState state;
            std::vector<size_t> inputOffsets(successor.node->getNumInputPorts());
            for (size_t port : utils::Range(successor.node->getNumInputPorts())) {
                auto driver = successor.node->getNonSignalDriver(port);
                if (driver.node != nullptr) {
                    auto conType = hlim::getOutputConnectionType(driver);
                    size_t offset = state.size();
                    state.resize(offset + (conType.width + 63)/64 * 64);
                    inputOffsets[port] = offset;
                    if (Node_Constant *constNode = dynamic_cast<Node_Constant*>(driver.node)) {
                        size_t arr[] = {offset};
                        constNode->simulateReset(ignoreCallbacks, state, nullptr, arr); // import const data
                    } else {
                        state.clearRange(sim::DefaultConfig::DEFINED, offset, conType.width);   // set non-const data to undefined
                    }
                }
            }

            // Allocate Outputs
            std::vector<size_t> outputOffsets(successor.node->getNumOutputPorts());
            for (size_t port : utils::Range(successor.node->getNumOutputPorts())) {
                auto conType = successor.node->getOutputConnectionType(port);
                size_t offset = state.size();
                state.resize(offset + (conType.width + 63)/64 * 64);
                outputOffsets[port] = offset;
            }

            // Simulate node
            successor.node->simulateEvaluate(ignoreCallbacks, state, nullptr, inputOffsets.data(), outputOffsets.data()); // compute outputs

            // Check all outputs. If any are fully defined, all nodes connected to that output can instead be connected to a const-node with the result.
            // If this nodes ends up without any other nodes connected to it, it will be culled by other optimization steps.
            for (size_t port : utils::Range(successor.node->getNumOutputPorts())) {
                auto conType = successor.node->getOutputConnectionType(port);

                bool allDefined = true;
                for (auto i : utils::Range(conType.width))
                    if (!state.get(sim::DefaultConfig::DEFINED, outputOffsets[port]+i)) {
                        allDefined = false;
                        break;
                    }

                if (allDefined) {
                    //std::cout << "    Found all const output" << std::endl;

                    auto* constant = createNode<Node_Constant>(state.extract(outputOffsets[port], conType.width), conType.interpretation);
                    constant->moveToGroup(successor.node->getGroup());
                    NodePort newConstOutputPort{.node = constant, .port = 0};

                    while (!successor.node->getDirectlyDriven(port).empty()) {
                        NodePort input = successor.node->getDirectlyDriven(port).back();
                        input.node->connectInput(input.port, newConstOutputPort);
                    }

                    // Add the new const-node output to the openList to continue const-propagation from here.
                    openList.push_back(newConstOutputPort);
                }
            }
        }
    }
}

void Circuit::removeFalseLoops()
{
//    for (size_t i = 0; i < m_nodes.size(); i++)
//    {
//        auto* signalNode = dynamic_cast<const Node_Signal*>(m_nodes[i].get());
//
//        if (signalNode && !signalNode->getNonSignalDriver(0).node)
//        {
//            m_nodes[i] = std::move(m_nodes.back());
//            m_nodes.pop_back();
//            i--;
//        }
//    }
}

/// @details It seems many parts of the vhdl export still require signal nodes so this step adds back in missing ones
void Circuit::ensureSignalNodePlacement()
{
    std::map<NodePort, Node_Signal*> addedSignalsNodes;

    for (auto idx : utils::Range(m_nodes.size())) {
        auto node = m_nodes[idx].get();
        if (dynamic_cast<Node_Signal*>(node) != nullptr) continue;
        for (auto i : utils::Range(node->getNumInputPorts())) {
            auto driver = node->getDriver(i);
            if (driver.node == nullptr) continue;
            if (node->getDriverConnType(i).interpretation == ConnectionType::DEPENDENCY) continue;
            if (dynamic_cast<Node_Signal*>(driver.node) != nullptr) continue;
            if (driver.node->getGroup() != nullptr && driver.node->getGroup()->getGroupType() == NodeGroup::GroupType::SFU) continue;

            auto it = addedSignalsNodes.find(driver);
            if (it != addedSignalsNodes.end()) {
                node->connectInput(i, {.node=it->second, .port=0ull});
            } else {
                auto *sigNode = createNode<Node_Signal>();
                sigNode->moveToGroup(driver.node->getGroup());

                sigNode->connectInput(driver);
                node->connectInput(i, {.node=sigNode, .port=0ull});
                addedSignalsNodes[driver] = sigNode;
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
            propagateConstants();
            cullOrphanedSignalNodes();
            cullUnnamedSignalNodes();
            cullSequentiallyDuplicatedSignalNodes();
            mergeMuxes();
            removeIrrelevantMuxes();
            cullMuxConditionNegations();
            removeNoOps();
            foldRegisterMuxEnableLoops();
            removeConstSelectMuxes();
            propagateConstants(); // do again after muxes are removed
            cullUnusedNodes();
            ensureSignalNodePlacement();

            findMemoryGroups(*this);
            buildExplicitMemoryCircuitry(*this);
            cullUnnamedSignalNodes();
            cullUnusedNodes(); // do again after memory group extraction with potential register retiming

            inferSignalNames();
        break;
    };
    m_root->reccurInferInstanceNames();
}


Node_Signal *Circuit::appendSignal(NodePort &nodePort)
{
    Node_Signal *sig = createNode<Node_Signal>();
    sig->recordStackTrace();
    if (nodePort.node != nullptr) {
        sig->connectInput(nodePort);
        sig->moveToGroup(nodePort.node->getGroup());
    }
    nodePort = {.node=sig, .port=0ull};
    return sig;
}


}
