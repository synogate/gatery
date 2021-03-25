#include "Node_Multiplexer.h"


namespace hcl::core::hlim {

void Node_Multiplexer::connectInput(size_t operand, const NodePort &port)
{
    NodeIO::connectInput(1+operand, port);
    if (port.node != nullptr)
        setOutputConnectionType(0, port.node->getOutputConnectionType(port.port));
}

void Node_Multiplexer::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    auto selectorDriver = getDriver(0);
    if (inputOffsets[0] == ~0ull) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }

    const auto &selectorType = selectorDriver.node->getOutputConnectionType(selectorDriver.port);
    HCL_ASSERT_HINT(selectorType.width <= 64, "Multiplexer with more than 64 bit selector not possible!");

    if (!allDefinedNonStraddling(state, inputOffsets[0], selectorType.width)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }

    std::uint64_t selector = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], selectorType.width);

    if (selector >= getNumInputPorts()-1) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
    if (inputOffsets[1+selector] != ~0ull)
        state.copyRange(outputOffsets[0], state, inputOffsets[1+selector], getOutputConnectionType(0).width);
    else
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
}



std::unique_ptr<BaseNode> Node_Multiplexer::cloneUnconnected() const
{
    std::unique_ptr<BaseNode> res(new Node_Multiplexer(getNumInputPorts()-1));
    copyBaseToClone(res.get());
    return res;
}


std::string Node_Multiplexer::attemptInferOutputName(size_t outputPort) const
{
#if 1
    std::string longestInput;
    for (auto i : utils::Range((size_t) 1, getNumInputPorts())) {
        auto driver = getDriver(i);
        if (driver.node == nullptr)
            continue;
        if (driver.node->getOutputConnectionType(driver.port).interpretation == ConnectionType::DEPENDENCY) continue;
        if (driver.node->getName().empty())
            continue;

        if (driver.node->getName().length() > longestInput.length())
            longestInput = driver.node->getName();
    }

    std::stringstream name;
    name << longestInput << "_mux";
    return name.str();
#else
    std::stringstream name;
    bool first = true;
    for (auto i : utils::Range((size_t) 1, getNumInputPorts())) {
        auto driver = getDriver(i);
        if (driver.node == nullptr)
            return "";
        if (driver.node->getOutputConnectionType(driver.port).interpretation == ConnectionType::DEPENDENCY) continue;
        if (driver.node->getName().empty()) {
            return "";
        } else {
            if (!first) name << '_';
            first = false;
            name << driver.node->getName();
        }
    }
    name << "_mux";
    return name.str();
#endif
}



}
