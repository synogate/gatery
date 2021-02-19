#include "FrontendUnitTestSimulationFixture.h"

#include <hcl/simulation/Simulator.h>

namespace hcl::core::frontend {

UnitTestSimulationFixture::~UnitTestSimulationFixture()
{
    // Force destruct of simulator (and all frontend signals held inside coroutines) before destruction of DesignScope in base class.
    m_simulator.reset(nullptr);
}

void UnitTestSimulationFixture::eval()
{
    sim::UnitTestSimulationFixture::eval(design.getCircuit());
}

void UnitTestSimulationFixture::runTicks(const hlim::Clock* clock, unsigned numTicks)
{
    sim::UnitTestSimulationFixture::runTicks(design.getCircuit(), clock, numTicks);
}

}
