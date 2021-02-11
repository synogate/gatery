#include "RunTimeSimulationContext.h"

#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/supportNodes/Node_SignalTap.h"

#include "Simulator.h"

namespace hcl::core::sim {

RunTimeSimulationContext::RunTimeSimulationContext(Simulator *simulator) : m_simulator(simulator)
{
}

void RunTimeSimulationContext::overrideSignal(hlim::NodePort output, const DefaultBitVectorState &state)
{
    auto *pin = dynamic_cast<hlim::Node_Pin*>(output.node);
    HCL_DESIGNCHECK_HINT(pin != nullptr, "Only io pin outputs allow run time overrides!");
    m_simulator->setInputPin(pin, state);
}

void RunTimeSimulationContext::getSignal(hlim::NodePort output, DefaultBitVectorState &state)
{
    auto *pin = dynamic_cast<hlim::Node_Pin*>(output.node);
    auto *sigTap = dynamic_cast<hlim::Node_SignalTap*>(output.node);
    HCL_DESIGNCHECK_HINT(pin || sigTap, "Only io pins and signal taps allow run time reading!");

    state = m_simulator->getValueOfOutput(output);
}

void RunTimeSimulationContext::simulationFiberSuspending(std::coroutine_handle<> handle, WaitFor &waitFor)
{
    m_simulator->simulationFiberSuspending(handle, waitFor, {});
}

void RunTimeSimulationContext::simulationFiberSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil)
{
    m_simulator->simulationFiberSuspending(handle, waitUntil, {});
}


}