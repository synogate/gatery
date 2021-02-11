#include "SimulationFiber.h"

namespace hcl::core::sim {

SimulationFiber::SimulationFiber(Handle handle) : m_handle(handle)
{
}

SimulationFiber::~SimulationFiber()
{
    if (m_handle)
        m_handle.destroy();
}

void SimulationFiber::resume()
{
    if (!m_handle.done())
        m_handle.resume();
}

}
