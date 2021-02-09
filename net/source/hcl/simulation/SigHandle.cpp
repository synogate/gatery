#include "SigHandle.h"

#include "SimulationContext.h"
#include "../hlim/Node.h"

namespace hcl::core::sim {

void SigHandle::operator=(std::uint64_t v)
{
    auto width = m_output.node->getOutputConnectionType(m_output.port).width;
    HCL_ASSERT(width <= 64);
    DefaultBitVectorState state;
    state.resize(width);
    state.data(DefaultConfig::DEFINED)[0] = 0;
    state.setRange(DefaultConfig::DEFINED, 0, width);
    state.data(DefaultConfig::VALUE)[0] = v;

    SimulationContext::current()->overrideSignal(m_output, state);
}

void SigHandle::operator=(const DefaultBitVectorState &state)
{
    SimulationContext::current()->overrideSignal(m_output, state);
}


std::uint64_t SigHandle::value() const
{
    auto width = m_output.node->getOutputConnectionType(m_output.port).width;
    HCL_ASSERT(width <= 64);
    DefaultBitVectorState state;
    SimulationContext::current()->getSignal(m_output, state);
    return state.extractNonStraddling(DefaultConfig::VALUE, 0, width);
}

DefaultBitVectorState SigHandle::eval() const
{
    DefaultBitVectorState state;
    SimulationContext::current()->getSignal(m_output, state);
    return state;
}


std::uint64_t SigHandle::defined() const
{
    auto width = m_output.node->getOutputConnectionType(m_output.port).width;
    HCL_ASSERT(width <= 64);
    DefaultBitVectorState state;
    SimulationContext::current()->getSignal(m_output, state);
    return state.extractNonStraddling(DefaultConfig::DEFINED, 0, width);
}

}