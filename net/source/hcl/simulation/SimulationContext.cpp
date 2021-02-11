#include "SimulationContext.h"

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

namespace hcl::core::sim {

thread_local SimulationContext *SimulationContext::m_current = nullptr;


SimulationContext::SimulationContext()
{
    m_overshadowed = m_current;
    m_current = this;
}

SimulationContext::~SimulationContext()
{
    m_current = m_overshadowed;
}


}