#include "Node_SignalTap.h"

#include <boost/format.hpp>

#include <sstream>

namespace hcl::core::hlim {

void Node_SignalTap::addInput(hlim::NodePort input)
{
    resizeInputs(getNumInputPorts()+1);
    connectInput(getNumInputPorts()-1, input);
}

void Node_SignalTap::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    bool triggered;
    if (m_trigger == TRIG_ALWAYS) {
        triggered = true;
    } else {
        HCL_ASSERT_HINT(getNumInputPorts() > 0, "Missing condition input signal!");
        auto conditionDriver = getNonSignalDriver(0);
        if (conditionDriver.node == nullptr) {
            triggered = true; // unconnected is undefined and undefined always triggers.
        } else {
            const auto &conditionType = conditionDriver.node->getOutputConnectionType(conditionDriver.port);
            HCL_ASSERT_HINT(conditionType.width == 1, "Condition must be 1 bit!");
            
            bool defined = state.get(sim::DefaultConfig::DEFINED, inputOffsets[0]);
            bool value = state.get(sim::DefaultConfig::VALUE, inputOffsets[0]);
            
            triggered = !defined || (m_trigger == TRIG_FIRST_INPUT_HIGH && value) || (m_trigger == TRIG_FIRST_INPUT_LOW && !value);
        }
    }
    
    if (triggered) {
        struct Concatenator : public boost::static_visitor<> {
            void operator()(const std::string &str) {
                message << str;
            }
            void operator()(const FormattedSignal &signal) {  /// @todo: invoke pretty printer
                auto driver = node->getNonSignalDriver(signal.inputIdx);
                if (driver.node == nullptr) {
                    message << "UNCONNECTED";
                    return;
                }
                const auto &type = driver.node->getOutputConnectionType(driver.port);
                for (auto i : utils::Range(type.width)) {
                    if (state.get(sim::DefaultConfig::DEFINED, inputOffsets[signal.inputIdx]+type.width-1-i)) {
                        if (state.get(sim::DefaultConfig::VALUE, inputOffsets[signal.inputIdx]+type.width-1-i))
                            message << '1';
                        else
                            message << '0';
                    } else
                        message << 'X';
                }
            }
            
            const Node_SignalTap *node;
            sim::DefaultBitVectorState &state;
            const size_t *inputOffsets;
            std::stringstream message;
            
            Concatenator(const Node_SignalTap *node, sim::DefaultBitVectorState &state, const size_t *inputOffsets) : node(node), state(state), inputOffsets(inputOffsets) { }
        };
        
        Concatenator concatenator(this, state, inputOffsets);
        for (const auto &part : m_logMessage)
            boost::apply_visitor(concatenator, part);
        
        switch (m_level) {
            case LVL_ASSERT: 
                simCallbacks.onAssert(this, concatenator.message.str());
            break;
            case LVL_WARN: 
                simCallbacks.onWarning(this, concatenator.message.str());
            break;
            case LVL_DEBUG: 
                simCallbacks.onDebugMessage(this, concatenator.message.str());
            break;
        }
    }
}

std::string Node_SignalTap::getTypeName() const
{
    switch (m_level) {
        case LVL_ASSERT: return "sig_tap_assert";
        case LVL_WARN: return "sig_tap_warn";
        case LVL_DEBUG: return "sig_tap_debug";
        default: return "sig_tap";
    }
}

void Node_SignalTap::assertValidity() const
{
}

std::string Node_SignalTap::getInputName(size_t idx) const
{
    if (m_trigger != TRIG_ALWAYS && idx == 0)
        return "trigger";
    return (boost::format("input_%d") % idx).str();
}

std::string Node_SignalTap::getOutputName(size_t idx) const
{
    return "";
}


std::vector<size_t> Node_SignalTap::getInternalStateSizes() const
{
    return {};
}


}
