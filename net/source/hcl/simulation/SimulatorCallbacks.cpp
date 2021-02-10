#include "SimulatorCallbacks.h"

#include "../hlim/Clock.h"

#include <iostream>

namespace hcl::core::sim {

void SimulatorConsoleOutput::onNewTick(const hlim::ClockRational &simulationTime)
{
    std::cout << "New simulation tick: " << simulationTime << std::endl;
}

void SimulatorConsoleOutput::onClock(const hlim::Clock *clock, bool risingEdge)
{

    std::cout << "Clock " << clock->getName() << " has " << (risingEdge?"rising":"falling") << " edge." << std::endl;
}

void SimulatorConsoleOutput::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
    std::cout << msg << std::endl;
}

void SimulatorConsoleOutput::onWarning(const hlim::BaseNode *src, std::string msg)
{
    std::cout << msg << std::endl;
}

void SimulatorConsoleOutput::onAssert(const hlim::BaseNode *src, std::string msg)
{
    std::cout << msg << std::endl;
}


}
