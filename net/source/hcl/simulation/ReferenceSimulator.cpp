#include "ReferenceSimulator.h"
#include "BitAllocator.h"

#include "../utils/Range.h"
#include "../hlim/Circuit.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Compare.h"
#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Arithmetic.h"
#include "../hlim/coreNodes/Node_Logic.h"
#include "../hlim/coreNodes/Node_Multiplexer.h"
#include "../hlim/coreNodes/Node_PriorityConditional.h"
#include "../hlim/coreNodes/Node_Rewire.h"
#include "../hlim/NodeVisitor.h"

#include <iostream>

#include <immintrin.h>

namespace hcl::core::sim {
    
    
void ExecutionBlock::evaluate(SimulatorCallbacks &simCallbacks, DataState &state) const
{
    for (const auto &step : m_steps) 
        step.node->simulateEvaluate(simCallbacks, state.signalState, step.internal.data(), step.inputs.data(), step.outputs.data());
}
    
void ExecutionBlock::addStep(MappedNode mappedNode)
{
    m_steps.push_back(mappedNode);
}

ClockedNode::ClockedNode(MappedNode mappedNode, size_t clockPort) : m_mappedNode(std::move(mappedNode)), m_clockPort(clockPort)
{
}

void ClockedNode::advance(SimulatorCallbacks &simCallbacks, DataState &state) const
{
    m_mappedNode.node->simulateAdvance(simCallbacks, state.signalState, m_mappedNode.internal.data(), m_mappedNode.outputs.data(), m_clockPort);
}

    
void Program::compileProgram(const hlim::Circuit &circuit, const std::vector<hlim::BaseNode*> &nodes)
{
    allocateSignals(circuit, nodes);
    
    
    for (const auto &clock : circuit.getClocks()) {
        m_stateMapping.clockToClkDomain[clock.get()] = m_clockDomains.size();
        m_clockDomains.push_back({
            .clockedNodes={}
        });
    }

    
    std::set<hlim::NodePort> outputsReady;
    
    std::set<hlim::BaseNode*> nodesRemaining;
    for (auto node : nodes) {
        if (dynamic_cast<hlim::Node_Signal*>(node) != nullptr) continue;
        nodesRemaining.insert(node);
        
        MappedNode mappedNode;
        mappedNode.node = node;
        mappedNode.internal = m_stateMapping.nodeToInternalOffset[node];
        for (auto i : utils::Range(node->getNumInputPorts()))
            mappedNode.inputs.push_back(m_stateMapping.outputToOffset[node->getNonSignalDriver(i)]);
        for (auto i : utils::Range(node->getNumOutputPorts())) 
            mappedNode.outputs.push_back(m_stateMapping.outputToOffset[{.node = node, .port = i}]);
        
        std::map<size_t, std::set<size_t>> clockDomainClockPortList;
        
        for (auto i : utils::Range(node->getNumOutputPorts())) {
            
            switch (node->getOutputType(i)) {
                case hlim::NodeIO::OUTPUT_IMMEDIATE:
                    // nothing
                break;
                case hlim::NodeIO::OUTPUT_CONSTANT: {
                    hlim::NodePort driver;
                    driver.node = node;
                    driver.port = i;
                    outputsReady.insert(driver);
                    
                    m_powerOnNodes.push_back(mappedNode);
                } break;
                case hlim::NodeIO::OUTPUT_LATCHED: {
                    hlim::NodePort driver;
                    driver.node = node;
                    driver.port = i;
                    outputsReady.insert(driver);

                    m_powerOnNodes.push_back(mappedNode);
                    
                    for (auto clockPort : utils::Range(node->getClocks().size())) {
                        size_t clockDomainIdx = m_stateMapping.clockToClkDomain[node->getClocks()[clockPort]];
                        clockDomainClockPortList[clockDomainIdx].insert(clockPort);
                    }
                } break;
            }            
        }
        
        for (const auto &pair : clockDomainClockPortList) {
            auto &clockDomain = m_clockDomains[pair.first];
            for (size_t clockPort : pair.second)
                clockDomain.clockedNodes.push_back(ClockedNode(mappedNode, clockPort));
            clockDomain.dependentExecutionBlocks.push_back(0ull); /// @todo only attach those that actually need to be recomputed
        }        
    }

    
    m_executionBlocks.push_back({});
    ExecutionBlock &execBlock = m_executionBlocks.back();
    
    while (!nodesRemaining.empty()) {
        hlim::BaseNode *readyNode = nullptr;
        for (auto node : nodesRemaining) {
            bool allInputsReady = true;
            for (auto i : utils::Range(node->getNumInputPorts())) {
                auto driver = node->getNonSignalDriver(i);
                if (driver.node != nullptr && (outputsReady.find(node->getNonSignalDriver(i)) == outputsReady.end())) {
                    allInputsReady = false;
                    break;
                }
            }
            
            if (allInputsReady) {
                readyNode = node;
                break;
            }
        }
        
        if (readyNode == nullptr) {
            std::cout << "nodesRemaining : " << nodesRemaining.size() << std::endl;
            
            for (auto node : nodesRemaining) {
                std::cout << node->getName() << "  " << node->getTypeName() << "  " << std::hex << (size_t)node << std::endl;
                for (auto i : utils::Range(node->getNumInputPorts())) {
                    auto driver = node->getNonSignalDriver(i);
                    if (driver.node != nullptr && (outputsReady.find(node->getNonSignalDriver(i)) == outputsReady.end())) {
                        std::cout << "    Input " << i << " not ready." << std::endl;
                        std::cout << "        " << driver.node->getName() << "  " << driver.node->getTypeName() << "  " << std::hex << (size_t)driver.node << std::endl;
                    }
                }
            }
        }
        
        HCL_DESIGNCHECK_HINT(readyNode != nullptr, "Cyclic dependency!");
        
        nodesRemaining.erase(readyNode);     
        
        MappedNode mappedNode;
        mappedNode.node = readyNode;
        mappedNode.internal = m_stateMapping.nodeToInternalOffset[readyNode];
        for (auto i : utils::Range(readyNode->getNumInputPorts()))
            mappedNode.inputs.push_back(m_stateMapping.outputToOffset[readyNode->getNonSignalDriver(i)]);
        for (auto i : utils::Range(readyNode->getNumOutputPorts())) 
            mappedNode.outputs.push_back(m_stateMapping.outputToOffset[{.node = readyNode, .port = i}]);
        
        execBlock.addStep(std::move(mappedNode));

        for (auto i : utils::Range(readyNode->getNumOutputPorts())) {
            hlim::NodePort driver;
            driver.node = readyNode;
            driver.port = i;
            outputsReady.insert(driver);
        }
    }
    
}

void Program::allocateSignals(const hlim::Circuit &circuit, const std::vector<hlim::BaseNode*> &nodes)
{
    m_stateMapping.clear();
    
    BitAllocator allocator;

    // variables
    for (auto node : nodes) {
        hlim::Node_Signal *signalNode = dynamic_cast<hlim::Node_Signal*>(node);
        if (signalNode != nullptr) {
            auto driver = signalNode->getNonSignalDriver(0);
            
            unsigned width = signalNode->getOutputConnectionType(0).width;
            
            if (driver.node != nullptr) {
                auto it = m_stateMapping.outputToOffset.find(driver);
                if (it == m_stateMapping.outputToOffset.end()) {
                    auto offset = allocator.allocate(width);
                    m_stateMapping.outputToOffset[driver] = offset;
                    m_stateMapping.outputToOffset[{.node = signalNode, .port = 0ull}] = offset;
                } else {
                    // point to same output port
                    m_stateMapping.outputToOffset[{.node = signalNode, .port = 0ull}] = it->second;
                }
            }
        } else {
            std::vector<size_t> internalSizes = node->getInternalStateSizes();
            std::vector<size_t> internalOffsets(internalSizes.size());
            for (auto i : utils::Range(internalSizes.size()))
                internalOffsets[i] = allocator.allocate(internalSizes[i]);
            m_stateMapping.nodeToInternalOffset[node] = internalOffsets;
            
            for (auto i : utils::Range(node->getNumOutputPorts())) {
                hlim::NodePort driver = {.node = node, .port = i};
                auto it = m_stateMapping.outputToOffset.find(driver);
                if (it == m_stateMapping.outputToOffset.end()) {
                    unsigned width = node->getOutputConnectionType(i).width;
                    m_stateMapping.outputToOffset[driver] = allocator.allocate(width);
                }
            }
        }
    }
    
    m_fullStateWidth = allocator.getTotalSize();
}


 
ReferenceSimulator::ReferenceSimulator()
{
}

void ReferenceSimulator::compileProgram(const hlim::Circuit &circuit, const std::set<hlim::NodePort> &outputs)
{
    std::vector<hlim::BaseNode*> nodes;
    if (outputs.empty()) {
        nodes.reserve(circuit.getNodes().size());
        for (const auto &node : circuit.getNodes())
            nodes.push_back(node.get());
    } else {
        std::set<hlim::BaseNode*> nodeSet;
        {
            std::vector<hlim::BaseNode*> stack;
            for (auto nodePort : outputs)
                stack.push_back(nodePort.node);
            
            while (!stack.empty()) {
                hlim::BaseNode *node = stack.back();
                stack.pop_back();
                if (nodeSet.find(node) == nodeSet.end()) {
                    nodeSet.insert(node);
                    for (auto i : utils::Range(node->getNumInputPorts())) 
                        if (node->getDriver(i).node != nullptr)
                            stack.push_back(node->getDriver(i).node);
                }
            }
        }
        nodes.reserve(nodeSet.size());
        for (const auto &node : nodeSet)
            nodes.push_back(node);
    }
    
    m_program.compileProgram(circuit, nodes);
}



 
void ReferenceSimulator::powerOn()
{
    m_simulationTime = 0;
    m_dataState.signalState.resize(m_program.m_fullStateWidth);
    
    m_dataState.signalState.clearRange(DefaultConfig::VALUE, 0, m_program.m_fullStateWidth);
    m_dataState.signalState.clearRange(DefaultConfig::DEFINED, 0, m_program.m_fullStateWidth);
    
    for (const auto &mappedNode : m_program.m_powerOnNodes)
        mappedNode.node->simulateReset(m_callbackDispatcher, m_dataState.signalState, mappedNode.internal.data(), mappedNode.outputs.data());
    
    m_dataState.clockState.resize(m_program.m_clockDomains.size());
    for (auto &cs : m_dataState.clockState)
        cs.high = false;

    for (auto &p : m_program.m_stateMapping.clockToClkDomain) {
        Event e;
        e.clock = p.first;
        e.clockDomainIdx = p.second;
        e.risingEdge = !m_dataState.clockState[e.clockDomainIdx].high;
        e.timeOfEvent = m_simulationTime + hlim::ClockRational(1,2) / e.clock->getAbsoluteFrequency();
        m_nextEvents.push(e);
    }

    reevaluate();
}

void ReferenceSimulator::reevaluate()
{
    /// @todo respect dependencies between blocks (once they are being expressed and made use of)
    for (auto &block : m_program.m_executionBlocks)
        block.evaluate(m_callbackDispatcher, m_dataState);
}

void ReferenceSimulator::advanceEvent()
{
    if (m_nextEvents.empty()) return;

    m_simulationTime = m_nextEvents.top().timeOfEvent;
    m_callbackDispatcher.onNewTick(m_simulationTime);

    std::set<size_t> triggeredExecutionBlocks;
    while (m_nextEvents.top().timeOfEvent == m_simulationTime) {
        auto event = m_nextEvents.top();
        m_nextEvents.pop();
        
        m_dataState.clockState[event.clockDomainIdx].high = event.risingEdge;

        auto trigType = event.clock->getTriggerEvent();
        if (trigType == hlim::Clock::TriggerEvent::RISING_AND_FALLING ||
            (trigType == hlim::Clock::TriggerEvent::RISING && event.risingEdge) ||
            (trigType == hlim::Clock::TriggerEvent::FALLING && !event.risingEdge)) {

            auto &clkDom = m_program.m_clockDomains[event.clockDomainIdx];

            for (auto id : clkDom.dependentExecutionBlocks)
                triggeredExecutionBlocks.insert(id);
            
            for (auto &cn : clkDom.clockedNodes)
                cn.advance(m_callbackDispatcher, m_dataState);
        }
        m_callbackDispatcher.onClock(event.clock, event.risingEdge);

        event.risingEdge = !event.risingEdge;
        event.timeOfEvent += hlim::ClockRational(1,2) / event.clock->getAbsoluteFrequency();
        m_nextEvents.push(event);
    }

    /// @todo respect dependencies between blocks (once they are being expressed and made use of)
    for (auto idx : triggeredExecutionBlocks)
        m_program.m_executionBlocks[idx].evaluate(m_callbackDispatcher, m_dataState);
}

void ReferenceSimulator::advance(hlim::ClockRational seconds)
{
    if (m_nextEvents.empty()) {
        m_simulationTime += seconds;
        return;
    }

    hlim::ClockRational targetTime = m_simulationTime + seconds;

    while (m_simulationTime < targetTime) {
        auto &nextEvent = m_nextEvents.top();
        if (nextEvent.timeOfEvent > targetTime) {
            m_simulationTime = targetTime;
            break;
        } else
            advanceEvent();
    }
}


bool ReferenceSimulator::outputOptimizedAway(const hlim::NodePort &nodePort)
{
    return !m_program.m_stateMapping.nodeToInternalOffset.contains(nodePort.node);
}


DefaultBitVectorState ReferenceSimulator::getValueOfInternalState(const hlim::BaseNode *node, size_t idx) 
{
    DefaultBitVectorState value;
    auto it = m_program.m_stateMapping.nodeToInternalOffset.find((hlim::BaseNode *) node);
    if (it == m_program.m_stateMapping.nodeToInternalOffset.end()) {
        value.resize(0);
    } else {
        unsigned width = node->getInternalStateSizes()[idx];
        value = m_dataState.signalState.extract(it->second[idx], width);
    }
    return value;
}

DefaultBitVectorState ReferenceSimulator::getValueOfOutput(const hlim::NodePort &nodePort)
{
    DefaultBitVectorState value;
    auto it = m_program.m_stateMapping.outputToOffset.find(nodePort);
    if (it == m_program.m_stateMapping.outputToOffset.end()) {
        value.resize(0);
    } else {
        unsigned width = nodePort.node->getOutputConnectionType(nodePort.port).width;
        value = m_dataState.signalState.extract(it->second, width);
    }
    return value;
}

std::array<bool, DefaultConfig::NUM_PLANES> ReferenceSimulator::getValueOfClock(const hlim::Clock *clk)
{
    std::array<bool, DefaultConfig::NUM_PLANES> res;

    auto it = m_program.m_stateMapping.clockToClkDomain.find((hlim::Clock *)clk);
    if (it == m_program.m_stateMapping.clockToClkDomain.end()) {
        res[DefaultConfig::DEFINED] = false;
        return res;
    }

    res[DefaultConfig::DEFINED] = true;
    res[DefaultConfig::VALUE] = m_dataState.clockState[it->second].high;
    return res;
}

/*
std::array<bool, DefaultConfig::NUM_PLANES> ReferenceSimulator::getValueOfReset(const std::string &reset)
{
    return {};
}
*/

}
