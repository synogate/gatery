#include "SimulationContext.h"

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

namespace hcl::core::sim {

thread_local SimulationContext *SimulationContext::m_current = nullptr;


SimulationContext::SimulationContext()
{
    HCL_ASSERT(m_current == nullptr);
    m_current = this;
}

SimulationContext::~SimulationContext()
{
    HCL_ASSERT(m_current == this);
    m_current = nullptr;
}


}