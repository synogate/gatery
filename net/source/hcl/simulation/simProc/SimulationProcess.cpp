#include "SimulationProcess.h"

namespace hcl::core::sim {

SimulationProcess::SimulationProcess(Handle handle) : m_handle(handle)
{
}

SimulationProcess::~SimulationProcess()
{
    if (m_handle)
        m_handle.destroy();
}

void SimulationProcess::resume()
{
    if (!m_handle.done())
        m_handle.resume();
}

}
