
#include "ConstructionTimeSimulationContext.h"

#include "ReferenceSimulator.h"
#include "BitAllocator.h"
#include "../hlim/Circuit.h"
#include "../hlim/Node.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Pin.h"


namespace hcl::core::sim {

void ConstructionTimeSimulationContext::overrideSignal(hlim::NodePort output, const DefaultBitVectorState &state)
{
    m_overrides[output] = state;
}

void ConstructionTimeSimulationContext::getSignal(hlim::NodePort output, DefaultBitVectorState &state)
{
    // Basic idea: Find and copy the combinatorial subnet. Then optimize and execute the subnet to find the value.
    hlim::Circuit simCircuit;

    std::vector<hlim::NodePort> inputPorts;

    std::map<hlim::NodePort, hlim::NodePort> outputsTranslated;
    std::set<hlim::NodePort> outputsHandled;
    std::vector<hlim::NodePort> openList;
    openList.push_back(output);

    // Find all inputs/limits to combinatorial subnets, create constant nodes for those inputs
    while (!openList.empty()) {
        auto nodePort = openList.back();
        openList.pop_back();

        if (outputsHandled.contains(nodePort)) continue;
        outputsHandled.insert(nodePort);

        // check overrides
        {
            auto it = m_overrides.find(nodePort);
            if (it != m_overrides.end()) {
                auto type = nodePort.node->getOutputConnectionType(nodePort.port);
                HCL_ASSERT(type.width == it->second.size());
                auto *c_node = simCircuit.createNode<hlim::Node_Constant>(it->second, type.interpretation);
                outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};

                for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
                    inputPorts.push_back(c);

                continue;
            }
        }

        // try use reset value
        if (auto *reg = dynamic_cast<hlim::Node_Register*>(nodePort.node)) {
            auto type = nodePort.node->getOutputConnectionType(nodePort.port);

            auto reset = reg->getNonSignalDriver(hlim::Node_Register::Input::RESET_VALUE);
            if (auto *const_v = dynamic_cast<hlim::Node_Constant*>(reset.node)) {
                auto *c_node = simCircuit.createUnconnectedClone(const_v);
                outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};
            } else
                outputsTranslated[nodePort] = {}; // translate to unconnected

            for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
                inputPorts.push_back(c);

            continue;
        }

        // use undefined for everything non-combinatorial
        if (!nodePort.node->isCombinatorial()) {
            auto type = nodePort.node->getOutputConnectionType(nodePort.port);

            DefaultBitVectorState undefinedState;
            undefinedState.resize(type.width);
            undefinedState.clearRange(DefaultConfig::DEFINED, 0, type.width);

            auto *c_node = simCircuit.createNode<hlim::Node_Constant>(std::move(undefinedState), type.interpretation);
            outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};

            for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
                inputPorts.push_back(c);

            continue;
        }

        // explore inputs
        for (auto i : utils::Range(nodePort.node->getNumInputPorts())) {
            auto driver = nodePort.node->getDriver(i);
            if (driver.node != nullptr)
                openList.push_back(driver);
        }
    }

    // Copy subnet
    std::map<hlim::BaseNode*, hlim::BaseNode*> mapSrc2Dst;
    simCircuit.copySubnet(inputPorts, {output}, mapSrc2Dst);

    // Link to const nodes
    for (auto np : inputPorts) {
        // only care about input ports to nodes that are part of the new subnet
        auto it = mapSrc2Dst.find(np.node);
        if (it != mapSrc2Dst.end()) {
            auto *oldConsumer = it->first;
            auto *newConsumer = it->second;

            // Translate the driver of that input
            auto oldDriver = oldConsumer->getDriver(np.port);
            auto newDriver = outputsTranslated.find(oldDriver)->second;

            // Rewire the corresponding consumer in the new subnet
            newConsumer->rewireInput(np.port, newDriver);
        }
    }

    // Translate the output of interest
    hlim::NodePort newOutput = output;
    newOutput.node = mapSrc2Dst.find(output.node)->second;

    // Force output's existance
    auto *pin = simCircuit.createNode<hlim::Node_Pin>();
    pin->connect(newOutput);

    // optimize
    simCircuit.optimize(3);

    // Run simulation
    sim::SimulatorCallbacks ignoreCallbacks;
    sim::ReferenceSimulator simulator;
    simulator.compileProgram(simCircuit);
    simulator.powerOn();
    simulator.reevaluate();

    // Fetch result
    state = simulator.getValueOfOutput(newOutput);
}

}