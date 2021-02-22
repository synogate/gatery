#include "Simulator.h"

namespace hcl::core::sim {


void Simulator::CallbackDispatcher::onNewTick(const hlim::ClockRational &simulationTime)
{
    for (auto *c : m_callbacks) c->onNewTick(simulationTime);
}

void Simulator::CallbackDispatcher::onClock(const hlim::Clock *clock, bool risingEdge)
{
    for (auto *c : m_callbacks) c->onClock(clock, risingEdge);
}

void Simulator::CallbackDispatcher::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
    for (auto *c : m_callbacks) c->onDebugMessage(src, msg);
}

void Simulator::CallbackDispatcher::onWarning(const hlim::BaseNode *src, std::string msg)
{
    for (auto *c : m_callbacks) c->onWarning(src, msg);
}

void Simulator::CallbackDispatcher::onAssert(const hlim::BaseNode *src, std::string msg)
{
    for (auto *c : m_callbacks) c->onAssert(src, msg);
}

void Simulator::CallbackDispatcher::onSimProcOutputOverridden(hlim::NodePort output, const DefaultBitVectorState &state)
{
    for (auto *c : m_callbacks) c->onSimProcOutputOverridden(output, state);
}

void Simulator::CallbackDispatcher::onSimProcOutputRead(hlim::NodePort output, const DefaultBitVectorState &state)
{
    for (auto *c : m_callbacks) c->onSimProcOutputRead(output, state);
}


}
